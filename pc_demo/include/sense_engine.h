#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sensevoice_pc {

// 配置（对应原仓库 hb_audio_capture 的参数）
struct EngineConfig {
  std::string model_path;    // sense-voice-small-fp16.gguf
  std::string config_path;   // cmd_word.json 所在目录
  std::string language = "zh";
  std::string wakeword = "\344\275\240\345\245\275";  // 你好
  int sample_rate = 16000;
  int n_threads = 4;
  bool enable_itn = true;   // 逆文本归一化（与 speech_engine 一致）
};

// 复刻自 sense-voice-frontend.h 的 VAD 常量
struct VadParams {
  int frame_size = 256;       // 16ms @16kHz
  int frame_shift = 128;      // 50% overlap
  float energy_threshold = 0.01f;
  float zcr_threshold = 0.2f;
};

// 识别/指令词结果回调（对应 AudioASRFunc / AudioCmdDataFunc）
struct EngineCallbacks {
  std::function<void(const std::string&)> on_asr;
  std::function<void(const std::string&)> on_cmd;
};

class SenseEngine {
 public:
  explicit SenseEngine(const EngineConfig& cfg);
  ~SenseEngine();

  bool Init(EngineCallbacks cbs);

  // 生产者：喂入一帧 PCM（16bit，单声道，-32768..32767）。等效 speech_engine::send_data
  void SendData(const std::vector<int16_t>& samples);
  // 离线文件模式：同步处理整段音频（不走异步队列），直接断句+识别
  void ProcessFile(const std::vector<int16_t>& pcm);
  // 强制输出当前已触发的语音段（离线文件末尾/进程退出前调用）
  void Flush();

  void Stop();

 private:
  void ProcessLoop();          // 消费者线程，等效 speech_engine::process()
  void FeedFrame(const std::vector<float>& mono);  // 处理一帧（VAD+断句）
  void ResetVad();             // 等效 vad_reset_state()
  bool RunSenseVoice(const std::vector<float>& audio, std::string& text);  // 等效 sense_voice_run()
  std::vector<std::string> LoadCmdWords();  // 读取 cmd_word.json

  EngineConfig cfg_;
  EngineCallbacks cbs_;
  VadParams vad_;

  std::thread proc_thread_;
  std::atomic<bool> running_{false};

  std::mutex queue_mtx_;
  std::condition_variable queue_cv_;
  std::vector<std::vector<int16_t>> queue_;
  static const size_t kMaxQueueSize = 256;  // 放宽以避免离线文件模式丢帧

  // VAD 状态
  bool vad_triggered_ = false;
  std::vector<float> vad_buf_;   // 累积的语音段（float 归一化 [-1,1]）
  uint64_t vad_start_ts_ = 0;    // 语音开始时间戳
  uint64_t silence_ms_ = 0;      // 当前静音累计

  std::vector<std::string> cmd_words_;
  int sent_speech_ms_ = 0;

  // 上游 sense_voice 句柄（opaque，避免包含大量 ggml 头）
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sensevoice_pc
