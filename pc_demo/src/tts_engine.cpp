// 离线 TTS 语音合成 + 播放（PC / WSL）。
// 参考 hobot_tts 的 "文本 -> PCM -> ALSA 播放" 思路，但后端换为 PC 可用的
// espeak-ng / Piper（地平线 wetts 为 aarch64 闭源库，x86 不可用）。

#include "tts_engine.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace sensevoice_pc {

namespace {

// 执行一条命令并等待完成，返回 exit code。
// 不再吞掉 stderr，TTS 合成/播放的错误直接透出，方便在终端排查。
int RunCmd(const std::string& cmd) {
  return std::system(cmd.c_str());
}

// 极简 Shell 转义：把单引号转义，整体用单引号包裹，避免文本里的特殊字符
// 破坏命令行。espeak-ng/piper 从 stdin 读这段 echo 出来的文本。
std::string ShellEscape(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') out += "'\\''";
    else out += c;
  }
  out += "'";
  return out;
}

std::string TempWavPath() {
  char tmpl[] = "/tmp/pc_tts_XXXXXX.wav";
  int fd = mkstemps(tmpl, 4);  // 末尾 4 字节 ".wav" 为后缀
  if (fd < 0) return "/tmp/pc_tts_fallback.wav";
  close(fd);
  return std::string(tmpl);
}

}  // namespace

TtsEngine::TtsEngine(const Config& cfg) : cfg_(cfg) {
  if (cfg_.engine == "espeak") {
    // espeak-ng-data 路径：若指定了 piper_dir，则复用其内置数据；
    // 否则依赖系统安装的 espeak-ng-data（/usr/share/espeak-ng-data）。
    if (!cfg_.piper_dir.empty()) {
      espeak_data_ = cfg_.piper_dir + "/espeak-ng-data";
    }
  }
}

TtsEngine::~TtsEngine() { Stop(); }

bool TtsEngine::Start() {
  if (running_) return true;
  running_ = true;
  stop_flag_ = false;
  worker_ = std::thread(&TtsEngine::Worker, this);
  return true;
}

bool TtsEngine::IsContentLess(const std::string& text) {
  // 判断文本是否"无实际语义内容"（仅空白 + 标点），避免对识别出的
  // "。" 这类结果做无意义合成/播放。
  // 注意：std::ispunct 只覆盖 ASCII 标点，对中文标点(。，、！？)不可靠，
  // 因此这里显式列出常见中文标点，逐字符判定。
  static const std::string cjk_punct = "。，、；：！？“”‘’（）《》〈〉【】…—·";
  for (char raw : text) {
    unsigned char c = static_cast<unsigned char>(raw);
    if (std::isspace(c)) continue;
    if (c < 0x80 && std::ispunct(c)) continue;          // ASCII 标点
    bool is_cjk_punct = false;
    for (char p : cjk_punct) if (raw == p) { is_cjk_punct = true; break; }
    if (is_cjk_punct) continue;
    return false;  // 遇到非空白非标点的字符 -> 有内容
  }
  return true;  // 全是空白/标点或无字符
}

void TtsEngine::Speak(const std::string& text) {
  if (text.empty() || IsContentLess(text)) return;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    q_.push(text);
  }
  cv_.notify_one();
}

void TtsEngine::Drain() {
  std::unique_lock<std::mutex> lock(mtx_);
  cv_.wait(lock, [this] { return q_.empty() && !running_; });
}

void TtsEngine::Stop() {
  if (!running_) return;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    stop_flag_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable()) worker_.join();
  running_ = false;
}

void TtsEngine::Worker() {
  while (true) {
    std::string text;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait(lock, [this] { return stop_flag_ || !q_.empty(); });
      if (stop_flag_ && q_.empty()) break;
      text = q_.front();
      q_.pop();
    }
    std::string wav = TempWavPath();
    if (Synthesize(text, wav)) {
      PlayWav(wav);
      std::remove(wav.c_str());
    }
  }
}

bool TtsEngine::Synthesize(const std::string& text, std::string& wav_path) {
  // piper_dir 下自带 libespeak-ng.so / libonnxruntime.so，必须加到 LD_LIBRARY_PATH
  std::string ld_prefix = cfg_.piper_dir.empty()
                              ? std::string()
                              : ("LD_LIBRARY_PATH=" + cfg_.piper_dir +
                                 (getenv("LD_LIBRARY_PATH")
                                      ? (":" + std::string(getenv("LD_LIBRARY_PATH")))
                                      : std::string()) + " ");

  if (cfg_.engine == "piper") {
    if (cfg_.piper_model.empty()) {
      std::fprintf(stderr, "[TTS] piper engine requires --piper-model\n");
      return false;
    }
    std::string piper_bin = cfg_.piper_dir.empty()
                                ? std::string("piper")
                                : (cfg_.piper_dir + "/piper");
    // piper 从 stdin 读文本，输出到 wav
    std::string cmd = "echo " + ShellEscape(text) + " | " + ld_prefix +
                      piper_bin + " --model " + cfg_.piper_model +
                      " --espeak_data " + cfg_.piper_dir + "/espeak-ng-data" +
                      " --output_file " + wav_path;
    if (RunCmd(cmd) != 0) {
      std::fprintf(stderr, "[TTS] piper synthesis failed\n");
      return false;
    }
    return true;
  }

  // 默认 espeak-ng：用 --stdout 输出 WAV 到文件。
  // 注意：不能用 -w，因为 -w 会初始化音频播放设备（WSLg/PulseAudio 下会挂起）；
  // --stdout 只做文本合成、不碰声卡，播放交由本类的 PlayWav（paplay）完成。
  std::string espeak_bin = cfg_.piper_dir.empty()
                               ? std::string("espeak-ng")
                               : (cfg_.piper_dir + "/espeak-ng");
  std::string cmd = ld_prefix + espeak_bin + " -v " + cfg_.voice + " -s " +
                    std::to_string(cfg_.rate) + " --stdout " +
                    ShellEscape(text) + " > " + wav_path;
  if (!espeak_data_.empty()) {
    cmd = "ESPEAK_DATA_PATH=" + espeak_data_ + " " + cmd;
  }
  if (RunCmd(cmd) != 0) {
    std::fprintf(stderr, "[TTS] espeak-ng synthesis failed\n");
    return false;
  }
  return true;
}

bool TtsEngine::PlayWav(const std::string& wav_path) {
  // WSLg 下 PA 默认设备名不是 "default"，直接用 paplay 内置默认最稳；
  // 仅当用户显式指定了设备时才加 --device。
  std::string cmd = cfg_.pa_device.empty()
                        ? std::string("paplay ") + wav_path
                        : ("paplay --device=" + cfg_.pa_device + " " + wav_path);
  if (RunCmd(cmd) != 0) {
    cmd = "aplay " + wav_path;
    if (RunCmd(cmd) != 0) {
      std::fprintf(stderr, "[TTS] playback failed (paplay & aplay)\n");
      return false;
    }
  }
  return true;
}

}  // namespace sensevoice_pc
