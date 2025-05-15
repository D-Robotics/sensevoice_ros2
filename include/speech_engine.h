
#ifndef INCLUDE_SPEECH_ENGINE_H_
#define INCLUDE_SPEECH_ENGINE_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <functional>

#include "sense-voice.h"
#include "silero-vad.h"

using AudioASRFunc = std::function<void(std::string)>;
using AudioCmdDataFunc = std::function<void(std::string)>;


// /**
//  * This the arbitrary data which will be passed to each callback.
//  * Later on we can for example add operation or tensor name filter from the CLI arg, or a file descriptor to dump the tensor.
//  */

// command-line parameters
struct sense_voice_params {
    int32_t n_threads     = std::min(5, (int32_t) std::thread::hardware_concurrency());
    int32_t n_processors  = 1;
    int32_t offset_t_ms   = 0;
    int32_t offset_n      = 0;
    int32_t duration_ms   = 0;
    int32_t progress_step = 5;
    int32_t max_context   = -1;
    int32_t max_len       = 0;
    int32_t n_mel       = 80;
    int32_t best_of       = sense_voice_full_default_params(SENSE_VOICE_SAMPLING_GREEDY).greedy.best_of;
    int32_t beam_size     = sense_voice_full_default_params(SENSE_VOICE_SAMPLING_BEAM_SEARCH).beam_search.beam_size;
    int32_t audio_ctx     = 0;

    float word_thold      =  0.01f;
    float entropy_thold   =  2.40f;
    float logprob_thold   = -1.00f;
    float grammar_penalty = 100.0f;
    float temperature     = 0.0f;
    float temperature_inc = 0.2f;

    // vad params
    float threshold      = 0.5f;
    float neg_threshold = 0.35f;
    int32_t min_speech_duration_ms = 250;
    int32_t max_speech_duration_ms = 5000;
    int32_t min_silence_duration_ms = 100;
    int32_t speech_pad_ms = 30;

    bool debug_mode      = false;
    bool translate       = false;
    bool detect_language = false;
    bool diarize         = false;
    bool tinydiarize     = false;
    bool split_on_word   = false;
    bool no_fallback     = false;
    bool output_txt      = false;
    bool output_vtt      = false;
    bool output_srt      = false;
    bool output_wts      = false;
    bool output_csv      = false;
    bool output_jsn      = false;
    bool output_jsn_full = false;
    bool output_lrc      = false;
    bool no_prints       = false;
    bool print_special   = false;
    bool print_colors    = false;
    bool print_progress  = false;
    bool no_timestamps   = false;
    bool log_score       = false;
    bool use_gpu         = true;
    bool flash_attn      = false;
    bool use_itn         = false;

    std::string language  = "zh";
    std::string prompt;
    std::string model     = "models/ggml-base.en.bin";

    std::string openvino_encode_device = "CPU";

    std::vector<std::string> fname_inp = {};
    std::vector<std::string> fname_out = {};

};

class speech_engine {
 public:
  static std::shared_ptr<speech_engine> &Instance() {
    static std::shared_ptr<speech_engine> engine;
    static std::once_flag init_flag;
    std::call_once(init_flag, []() {
      engine = std::shared_ptr<speech_engine>(new speech_engine());
    });
    return engine;
  }
  ~speech_engine(){}
  int Init(const std::string &cfg_path, const std::string &wakeup_name,
          std::shared_ptr<std::vector<std::string>> v_cmd_word,
          AudioASRFunc asr_func, AudioCmdDataFunc cmd_func);
  int DeInit();
  int Start();
  int Stop();

  void send_data(std::shared_ptr<std::vector<double>> data);
  void process(void);

 private:
  speech_engine(){}
  speech_engine(const speech_engine &);
  speech_engine &operator=(const speech_engine &);

 private:

  struct sense_voice_context * ctx = nullptr;
  sense_voice_full_params wparams;
  int sample_rate = 16000;
  struct sense_voice_params params;
  std::vector<float> vad_pad;
  int triggered = 0;
  std::shared_ptr<std::vector<double>> vad_data_ptr = nullptr;
  std::vector<double> vad_mute;

  std::thread process_thread;

  std::atomic<bool> stop_flag{false};
  static constexpr size_t kMaxQueueSize = 10;
  std::queue<std::pair<std::shared_ptr<std::vector<double>>, int>> process_queue;
  std::mutex mutex;
  std::condition_variable cv;

  bool enable_asr = true;
  bool asr_final = false;
  std::mutex web_mutex;
  std::string wakeup_name_;
  AudioASRFunc audio_asr_cb_ = nullptr;
  AudioASRFunc audio_cmd_cb_ = nullptr;
  std::shared_ptr<std::vector<std::string>> v_cmd_word_;

  std::chrono::system_clock::time_point vad_start;
  std::chrono::system_clock::time_point vad_pre;
  std::chrono::system_clock::time_point vad_send;
  std::chrono::system_clock::time_point vad_stop;
};

#endif
