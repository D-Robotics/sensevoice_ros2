#include "sense_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "sense-voice.h"  // 上游 lovemefan/SenseVoice.cpp（含 common.h）

namespace sensevoice_pc {

// ---------------------------------------------------------------------------
// 上游 SenseVoice 上下文（对应原仓库闭源 libsense-voice-core 的调用方式）
// ---------------------------------------------------------------------------
struct SenseEngine::Impl {
  struct sense_voice_context* ctx = nullptr;
  struct sense_voice_full_params params;
  bool ok = false;
};

namespace {
// 简单 JSON 字符串数组解析：["a", "b"]（不支持转义，仅供 cmd_word.json）
std::vector<std::string> ParseJsonStringArray(const std::string& text) {
  std::vector<std::string> out;
  std::string cur;
  bool in_str = false;
  for (char c : text) {
    if (c == '"') {
      if (in_str) { out.push_back(cur); cur.clear(); in_str = false; }
      else { in_str = true; }
    } else if (in_str) {
      cur += c;
    }
  }
  return out;
}

// 轻量能量 + 过零率 VAD（替代原仓库的 Silero-VAD，降低依赖）
bool EnergyZcrVad(const std::vector<float>& frame, float energy_th,
                  float zcr_th) {
  if (frame.empty()) return false;
  float energy = 0.0f;
  size_t zcr = 0;
  for (size_t i = 0; i < frame.size(); ++i) {
    energy += frame[i] * frame[i];
    if (i > 0 && ((frame[i] >= 0.0f) != (frame[i - 1] >= 0.0f))) ++zcr;
  }
  energy /= static_cast<float>(frame.size());
  float zcr_ratio = static_cast<float>(zcr) / static_cast<float>(frame.size());
  return energy > energy_th || zcr_ratio > zcr_th;
}
}  // namespace

SenseEngine::SenseEngine(const EngineConfig& cfg)
    : cfg_(cfg), impl_(std::make_unique<Impl>()) {}

SenseEngine::~SenseEngine() { Stop(); }

bool SenseEngine::Init(EngineCallbacks cbs) {
  cbs_ = std::move(cbs);
  cmd_words_ = LoadCmdWords();

  if (cfg_.model_path.empty()) {
    std::cerr << "[sense_engine] model_path is empty\n";
    return false;
  }

  // ---- 初始化（对应 speech_engine::Init 的步骤）----
  // 1) 默认上下文参数（use_gpu/flash_attn 由上游默认决定，这里仅开 use_itn）
  struct sense_voice_context_params cparams = sense_voice_context_default_params();
  cparams.use_gpu = false;
  cparams.flash_attn = false;
  cparams.use_itn = cfg_.enable_itn;  // 与 speech_engine 一致

  // 2) 加载模型
  impl_->ctx = sense_voice_small_init_from_file_with_params(cfg_.model_path.c_str(), cparams);
  if (!impl_->ctx) {
    std::cerr << "[sense_engine] failed to init sense_voice with model: "
              << cfg_.model_path << "\n";
    return false;
  }

  // 3) 语言
  impl_->ctx->language_id = sense_voice_lang_id(cfg_.language.c_str());

  // 4) 全量推理参数（对应 speech_engine 的 wparams）
  impl_->params = sense_voice_full_default_params(SENSE_VOICE_SAMPLING_GREEDY);
  impl_->params.n_threads = cfg_.n_threads;
  impl_->params.language = cfg_.language.c_str();
  impl_->params.no_timestamps = true;        // 只要纯文本
  impl_->params.single_segment = true;
  impl_->params.print_progress = false;
  impl_->params.print_timestamps = false;
  impl_->params.debug_mode = false;

  impl_->ok = true;
  running_ = true;
  proc_thread_ = std::thread(&SenseEngine::ProcessLoop, this);

  std::cout << "[sense_engine] init ok: model=" << cfg_.model_path
            << " lang=" << cfg_.language
            << " wakeword=" << cfg_.wakeword
            << " cmds=" << cmd_words_.size() << "\n";
  return true;
}

std::vector<std::string> SenseEngine::LoadCmdWords() {
  std::string path = cfg_.config_path;
  if (!path.empty() && path.back() != '/' && path.back() != '\\') path += "/";
  path += "cmd_word.json";
  std::ifstream f(path);
  if (!f) {
    std::cerr << "[sense_engine] cannot open cmd config: " << path << "\n";
    return {};
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return ParseJsonStringArray(ss.str());
}

// 对应 speech_engine::send_data —— 生产者
void SenseEngine::SendData(const std::vector<int16_t>& samples) {
  if (!running_) return;
  std::lock_guard<std::mutex> lk(queue_mtx_);
  if (queue_.size() >= kMaxQueueSize) return;
  queue_.push_back(samples);
  queue_cv_.notify_one();
}

// 强制将当前已触发的语音段作为一次识别输出（用于离线文件末尾/结束时）
void SenseEngine::Flush() {
  // 先确保队列里的数据都被消费者处理完
  {
    std::unique_lock<std::mutex> lk(queue_mtx_);
    queue_cv_.wait(lk, [this] { return queue_.empty(); });
  }
  // 若已有语音段在缓冲，强制断句识别
  if (vad_triggered_ && !vad_buf_.empty()) {
    int speech_ms = static_cast<int>(vad_buf_.size() * 1000.0 / cfg_.sample_rate);
    if (speech_ms >= 200) {
      std::string text;
      if (RunSenseVoice(vad_buf_, text)) {
        if (cbs_.on_asr) cbs_.on_asr(text);
        if (cbs_.on_cmd) {
          std::string cmd = "NONE";
          for (const auto& w : cmd_words_) {
            if (text.find(w) != std::string::npos) { cmd = w; break; }
          }
          cbs_.on_cmd(cmd);
        }
      }
    }
    ResetVad();
  }
}

void SenseEngine::Stop() {
  if (!running_) return;
  running_ = false;
  queue_cv_.notify_all();
  if (proc_thread_.joinable()) proc_thread_.join();
  if (impl_->ctx) {
    sense_voice_free_state(impl_->ctx->state);  // 上游提供的释放接口
    delete impl_->ctx;
    impl_->ctx = nullptr;
  }
}

void SenseEngine::ResetVad() {
  vad_triggered_ = false;
  vad_buf_.clear();
  silence_ms_ = 0;
}

// 对应 speech_engine::process() —— 消费者线程
void SenseEngine::ProcessLoop() {
  std::vector<int16_t> pending;
  while (running_) {
    {
      std::unique_lock<std::mutex> lk(queue_mtx_);
      queue_cv_.wait(lk, [this] { return !running_ || !queue_.empty(); });
      if (!running_ && queue_.empty()) break;
      pending = std::move(queue_.front());
      queue_.erase(queue_.begin());
    }

    // 双声道 → 单声道（原仓库默认采集 2 声道）
    std::vector<float> mono;
    mono.reserve(pending.size() / 2);
    if (/*channels==2 由数据长度判断*/ pending.size() > 1600) {
      for (size_t i = 0; i + 1 < pending.size(); i += 2) {
        mono.push_back((pending[i] + pending[i + 1]) * 0.5f / 32768.0f);
      }
    } else {
      for (int16_t s : pending) mono.push_back(s / 32768.0f);
    }
    if (mono.empty()) continue;

    const float energy_th = 0.004f;   // 静音阈值（能量）提高，减少中间误断
    const float zcr_th = 0.10f;       // 过零率阈值降低灵敏度
    bool speech = EnergyZcrVad(mono, energy_th, zcr_th);
    int frame_ms = static_cast<int>(mono.size() * 1000.0 / cfg_.sample_rate);

    if (speech && !vad_triggered_) {
      // 语音开始
      vad_triggered_ = true;
      silence_ms_ = 0;
      vad_buf_.clear();
      vad_buf_.insert(vad_buf_.end(), mono.begin(), mono.end());
    } else if (speech && vad_triggered_) {
      silence_ms_ = 0;
      vad_buf_.insert(vad_buf_.end(), mono.begin(), mono.end());
    } else if (!speech && vad_triggered_) {
      // 静音累计，超阈值断句（min_silence_duration_ms=1200，避免连说被切开）
      silence_ms_ += frame_ms;
      vad_buf_.insert(vad_buf_.end(), mono.begin(), mono.end());
      if (silence_ms_ > 1200) {
        // 过滤过短片段（< 400ms 多为气口/噪声，识别无意义）
        int speech_ms = static_cast<int>(vad_buf_.size() * 1000.0 / cfg_.sample_rate);
        if (speech_ms < 400) {
          ResetVad();
        } else {
          std::string text;
          if (RunSenseVoice(vad_buf_, text)) {
            if (cbs_.on_asr) cbs_.on_asr(text);
            if (cbs_.on_cmd) {
              std::string cmd = "NONE";
              for (const auto& w : cmd_words_) {
                if (text.find(w) != std::string::npos) { cmd = w; break; }
              }
              cbs_.on_cmd(cmd);
            }
          }
          ResetVad();
        }
      }
    }
  }
}

// 对应 speech_engine 中 sense_voice_full_parallel 之后的文本提取 + 唤醒词剥离
bool SenseEngine::RunSenseVoice(const std::vector<float>& audio, std::string& text) {
  if (!impl_->ok || !impl_->ctx || audio.empty()) return false;

  // 上游接口需要 std::vector<double>
  std::vector<double> samples(audio.begin(), audio.end());
  if (sense_voice_full_parallel(impl_->ctx, impl_->params, samples,
                                static_cast<int>(samples.size()), 1) != 0) {
    std::fprintf(stderr, "[sense_engine] sense_voice_full_parallel failed\n");
    return false;
  }

  // 从 ids 恢复文本（与 speech_engine.cpp 完全相同的方式）
  std::string raw;
  const auto& ids = impl_->ctx->state->ids;
  for (size_t i = 4; i < ids.size(); ++i) {
    int id = ids[i];
    if (i > 0 && ids[i - 1] == id) continue;   // 去重 token
    if (id) raw += impl_->ctx->vocab.id_to_token[id];
  }

  // 唤醒词剥离（对应 speech_engine：tmp_str.find(wakeup_name_) 后去掉前缀）
  if (!cfg_.wakeword.empty()) {
    if (raw.compare(0, cfg_.wakeword.size(), cfg_.wakeword) == 0) {
      raw = raw.substr(cfg_.wakeword.size());
    }
  }
  text = raw;
  return !text.empty();
}

}  // namespace sensevoice_pc
