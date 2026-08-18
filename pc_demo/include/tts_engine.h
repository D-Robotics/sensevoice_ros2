#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace sensevoice_pc {

// 离线 TTS 语音合成 + 播放封装（PC / WSL 可用）。
//
// 设计要点（参考 hobot_tts，但去掉对地平线闭源 wetts 的依赖）：
//   - 后端可选 espeak-ng（已内置、中文可用、音质偏机械）或 Piper（高质量，
//     需额外 onnx 模型）。默认 espeak-ng，模型就绪后加 --tts-engine piper 切换。
//   - 合成走子进程（espeak-ng / piper 可执行），输出临时 WAV；
//     播放走 PulseAudio（paplay），WSLg 下已验证可用。
//   - 内部一个后台播放线程串行消费文本队列，避免并发抢占声卡。
class TtsEngine {
 public:
  struct Config {
    std::string engine = "espeak";     // "espeak" | "piper"
    std::string piper_dir;             // piper 可执行/so/espeak-ng-data 所在目录
    std::string piper_model;           // Piper 的 .onnx 模型路径（engine=piper 时需要）
    std::string voice = "zh";          // espeak-ng 的 -v 参数，如 zh / zh-yue
    int rate = 175;                    // espeak-ng 语速（词/分）
    std::string pa_device;             // paplay 设备名（空=默认）
  };

  explicit TtsEngine(const Config& cfg);
  ~TtsEngine();

  // 启动后台播放线程。失败返回 false（但合成仍可用，仅没有声音）。
  bool Start();

  // 提交一段文本异步合成并播放。线程安全。
  void Speak(const std::string& text);

  // 阻塞等待队列播放完毕。
  void Drain();

  // 停止后台线程（会等待当前播放项结束）。
  void Stop();

 private:
  void Worker();
  bool Synthesize(const std::string& text, std::string& wav_path);
  bool PlayWav(const std::string& wav_path);

  Config cfg_;
  std::thread worker_;
  std::atomic<bool> running_{false};

  std::mutex mtx_;
  std::condition_variable cv_;
  std::queue<std::string> q_;
  bool stop_flag_ = false;

  // espeak-ng 数据目录（从 piper_dir 推导；若 engine=espeak 且未指定 piper_dir
  // 则依赖系统安装的 espeak-ng-data）
  std::string espeak_data_;
};

}  // namespace sensevoice_pc
