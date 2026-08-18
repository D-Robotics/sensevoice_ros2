// 离线 TTS 语音合成 + 播放（PC / WSL）。
// 参考 hobot_tts 的 "文本 -> PCM -> ALSA 播放" 思路，但后端换为 PC 可用的
// espeak-ng / Piper（地平线 wetts 为 aarch64 闭源库，x86 不可用）。

#include "tts_engine.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace sensevoice_pc {

namespace {

// 执行一条命令并等待完成，返回 exit code。stdout 被丢弃，stderr 透传。
int RunCmd(const std::string& cmd) {
  std::string full = cmd + " 2>/dev/null";
  return std::system(full.c_str());
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

void TtsEngine::Speak(const std::string& text) {
  if (text.empty()) return;
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
