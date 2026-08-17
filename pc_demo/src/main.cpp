// PC 版 SenseVoice 实时识别 demo
// 等效原仓库 hb_audio_capture 节点：ALSA 采集 -> VAD 断句 -> SenseVoice 识别
// 不依赖 ROS2 / 地平线闭源库。
//
// 用法示例:
//   ./sensevoice_pc_demo --model <sense-voice-small-fp16.gguf> \
//       --device plughw:0,0 --wakeword 你好 --lang zh \
//       --cmd-config ../config/cmd_word.json

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <signal.h>
#include <string>
#include <vector>

#include "alsa_audio.h"
#include "sense_engine.h"

using namespace sensevoice_pc;

static volatile sig_atomic_t g_stop = 0;
static void OnSignal(int) { g_stop = 1; }

struct Options {
  std::string model;
  std::string device = "plughw:0,0";
  std::string file;   // 可选：WAV 文件离线识别（不依赖麦克风）
  std::string wakeword = "\344\275\240\345\245\275";  // 你好
  std::string lang = "zh";
  std::string cmd_config = "config/cmd_word.json";
  int threads = 4;
  int channels = 2;   // 原仓库默认采集 2 声道，引擎内降混
  int rate = 16000;
};

// 读取 16-bit PCM WAV，重采样/转单声道到 16k，返回 int16 采样序列。
// 支持常见采样率（用简单线性重采样）；返回 false 表示解析失败。
static bool ReadWavToMono16k(const std::string& path, std::vector<int16_t>& out,
                             int target_rate = 16000) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) { std::fprintf(stderr, "[wav] cannot open %s\n", path.c_str()); return false; }

  auto rd_u32 = [&](uint32_t& v) -> bool {
    unsigned char b[4];
    if (std::fread(b, 1, 4, f) != 4) return false;
    v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return true;
  };
  auto rd_u16 = [&](uint16_t& v) -> bool {
    unsigned char b[2];
    if (std::fread(b, 1, 2, f) != 2) return false;
    v = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
    return true;
  };
  auto rd_tag = [&](char tag[4]) -> bool {
    return std::fread(tag, 1, 4, f) == 4;
  };

  char riff[4], wave[4];
  uint32_t riff_size;
  if (!rd_tag(riff) || !rd_u32(riff_size) || !rd_tag(wave)) { std::fclose(f); return false; }
  if (std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0) {
    std::fprintf(stderr, "[wav] not a RIFF/WAVE file\n"); std::fclose(f); return false;
  }

  int ch = 0, sr = 0, bits = 0;
  uint32_t data_size = 0;
  bool found_fmt = false, found_data = false;
  // 遍历所有 chunk
  while (true) {
    char tag[4]; uint32_t sz;
    if (!rd_tag(tag) || !rd_u32(sz)) break;
    if (std::strncmp(tag, "fmt ", 4) == 0) {
      uint16_t audio_fmt, nch, block_align, nbits;
      uint32_t sample_rate, byte_rate;
      if (!rd_u16(audio_fmt) || !rd_u16(nch) || !rd_u32(sample_rate) ||
          !rd_u32(byte_rate) || !rd_u16(block_align) || !rd_u16(nbits)) { break; }
      ch = nch; sr = (int)sample_rate; bits = nbits; found_fmt = true;
      // 跳过 fmt 扩展字节
      long extra = (long)sz - 16;
      if (extra > 0) std::fseek(f, extra, SEEK_CUR);
    } else if (std::strncmp(tag, "data", 4) == 0) {
      data_size = sz; found_data = true; break;
    } else {
      std::fseek(f, (long)sz, SEEK_CUR);
    }
  }
  if (!found_fmt || !found_data) { std::fprintf(stderr, "[wav] missing fmt/data\n"); std::fclose(f); return false; }
  if (bits != 16) { std::fprintf(stderr, "[wav] only 16-bit supported (got %d)\n", bits); std::fclose(f); return false; }
  if (ch <= 0 || sr <= 0) { std::fprintf(stderr, "[wav] bad channels/sample_rate\n"); std::fclose(f); return false; }

  std::vector<int16_t> raw(data_size / 2);
  if (std::fread(raw.data(), 2, raw.size(), f) != raw.size()) {
    std::fprintf(stderr, "[wav] data read incomplete\n"); std::fclose(f); return false;
  }
  std::fclose(f);

  // 转单声道
  std::vector<int16_t> mono;
  mono.reserve(raw.size() / ch + 1);
  for (size_t i = 0; i + ch <= raw.size(); i += ch) {
    int32_t s = 0;
    for (int c = 0; c < ch; ++c) s += raw[i + c];
    mono.push_back(static_cast<int16_t>(s / ch));
  }

  // 重采样到 target_rate（线性插值）
  if (sr == target_rate) { out = std::move(mono); return true; }
  double ratio = static_cast<double>(target_rate) / sr;
  size_t nout = static_cast<size_t>(mono.size() * ratio);
  out.resize(nout);
  for (size_t i = 0; i < nout; ++i) {
    double src = i / ratio;
    size_t i0 = static_cast<size_t>(src);
    size_t i1 = std::min(i0 + 1, mono.size() - 1);
    double frac = src - i0;
    double v = mono[i0] * (1 - frac) + mono[i1] * frac;
    out[i] = static_cast<int16_t>(v);
  }
  return true;
}

static void PrintUsage(const char* prog) {
  std::printf(
      "Usage: %s [options]\n"
      "  --model <path>        sense-voice-small-fp16.gguf (required)\n"
      "  --device <name>       ALSA capture device, default plughw:0,0\n"
      "  --file <path>         offline WAV file recognition (skip mic)\n"
      "  --wakeword <word>     wake word to strip, default 你好\n"
      "  --lang <code>         language, default zh\n"
      "  --cmd-config <path>   cmd_word.json path, default config/cmd_word.json\n"
      "  --threads <n>         inference threads, default 4\n"
      "  --channels <1|2>      mic channels, default 2\n"
      "  --rate <hz>           sample rate, default 16000\n"
      "  -h, --help            show this help\n",
      prog);
}

static bool ParseArgs(int argc, char** argv, Options& opt) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&](const char* name) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "missing value for %s\n", name);
        std::exit(1);
      }
      return argv[++i];
    };
    if (a == "--model") opt.model = next("--model");
    else if (a == "--device") opt.device = next("--device");
    else if (a == "--file") opt.file = next("--file");
    else if (a == "--wakeword") opt.wakeword = next("--wakeword");
    else if (a == "--lang") opt.lang = next("--lang");
    else if (a == "--cmd-config") opt.cmd_config = next("--cmd-config");
    else if (a == "--threads") opt.threads = std::atoi(next("--threads"));
    else if (a == "--channels") opt.channels = std::atoi(next("--channels"));
    else if (a == "--rate") opt.rate = std::atoi(next("--rate"));
    else if (a == "-h" || a == "--help") { PrintUsage(argv[0]); std::exit(0); }
    else { std::fprintf(stderr, "unknown option: %s\n", a.c_str()); return false; }
  }
  return true;
}

int main(int argc, char** argv) {
  Options opt;
  if (!ParseArgs(argc, argv, opt)) { PrintUsage(argv[0]); return 1; }
  if (opt.model.empty()) {
    std::fprintf(stderr, "error: --model is required\n");
    PrintUsage(argv[0]);
    return 1;
  }

  signal(SIGINT, OnSignal);
  signal(SIGTERM, OnSignal);

  // ---- 初始化识别引擎（等效 speech_engine::Init）----
  EngineConfig ecfg;
  ecfg.model_path = opt.model;
  ecfg.config_path = opt.cmd_config.substr(0, opt.cmd_config.find_last_of("/\\"));
  ecfg.language = opt.lang;
  ecfg.wakeword = opt.wakeword;
  ecfg.n_threads = opt.threads;
  ecfg.sample_rate = opt.rate;

  SenseEngine engine(ecfg);
  bool init_ok = engine.Init({
      [](const std::string& text) {
        std::printf("\n[ASR] %s\n", text.c_str());
        std::fflush(stdout);
      },
      [](const std::string& cmd) {
        std::printf("[CMD] %s\n", cmd.c_str());
        std::fflush(stdout);
      },
  });
  if (!init_ok) {
    std::fprintf(stderr, "engine init failed\n");
    return 2;
  }

  std::vector<int16_t> buf;
  size_t frames = 0;

  if (!opt.file.empty()) {
    // ---- 离线文件模式：读 WAV，分帧模拟实时流 ----
    std::vector<int16_t> pcm;
    if (!ReadWavToMono16k(opt.file, pcm, opt.rate)) {
      return 4;
    }
    std::printf("[main] file mode: %s, %zu samples (%.1fs @%d)\n",
                opt.file.c_str(), pcm.size(),
                pcm.size() / static_cast<double>(opt.rate), opt.rate);
    const size_t chunk = 512;  // 与 mic 帧大小一致
    for (size_t off = 0; off < pcm.size(); off += chunk) {
      if (g_stop) break;
      size_t n = std::min(chunk, pcm.size() - off);
      buf.assign(pcm.begin() + off, pcm.begin() + off + n);
      engine.SendData(buf);
    }
    // 强制输出最后一段语音（不依赖静音断句）
    engine.Flush();
    std::printf("[main] file consumed\n");
  } else {
    // ---- 实时麦克风模式 ----
    AlsaAudio mic;
    if (!mic.Open(opt.device, opt.rate, opt.channels)) {
      std::fprintf(stderr, "open mic failed\n");
      return 3;
    }
    std::printf("[main] mic opened: %s rate=%d ch=%d, say wake word \"%s\" then command...\n",
                opt.device.c_str(), mic.rate(), mic.channels(), opt.wakeword.c_str());
    while (!g_stop) {
      if (!mic.Read(buf, frames)) continue;
      if (frames == 0) continue;
      engine.SendData(buf);
    }
  }

  engine.Stop();
  std::printf("\n[main] bye\n");
  return 0;
}
