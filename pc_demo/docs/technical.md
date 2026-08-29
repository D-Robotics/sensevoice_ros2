# pc_demo 技术文档

> 本文档面向开发者，说明 `pc_demo` 的设计目标、目录结构、模块实现、构建运行与已知限制。
> 操作指南（命令、参数表、下载地址）见同级 `../README.md`。

---

## 1. 项目定位

`pc_demo` 是 `sensevoice_ros2` 仓库中 **面向 x86_64 PC（Ubuntu 22.04）的独立演示程序** ，
在 PC 上以纯开源、纯 CPU 的方式复刻本仓库 `speech_engine` 的核心算法逻辑：

```
麦克风 / WAV 采集  →  VAD 断句  →  SenseVoice 识别  →  唤醒词剥离  →  指令词匹配
```

关键差异（相对 RDK 板上的 ROS2 节点）：

| 维度 | RDK 原仓库 | pc_demo |
|---|---|---|
| 运行环境 | 地平线 RDK（aarch64） | 任意 x86_64 Linux（含 WSL2） |
| 识别引擎 | 闭源 `libsense-voice-core` | 开源 `lovemefan/SenseVoice.cpp`（ggml，纯 CPU） |
| 进程模型 | ROS2 节点 + 话题 | 单进程命令行程序，仅打印文本 |
| VAD | Silero-VAD（onnx） | 轻量能量 + 过零率 VAD（无外部依赖） |
| 语音播报 | `hobot_tts`（闭源 `wetts`） | espeak-ng / Piper（均可 PC 运行） |

**设计原则**：与 `sensevoice_ros2` 保持「行为一致」而非「二进制一致」——
唤醒词剥离规则、指令词匹配方式、参数默认值均对齐原仓库，但底层实现替换为 PC 可编译的开源组件。

---

## 2. 目录结构

```
pc_demo/
├── README.md                 # 用户向：构建/运行/参数/下载
├── docs/technical.md         # 本文档（开发者向）
├── CMakeLists.txt            # 构建定义（含上游子目录接入）
├── build.sh                  # 一键构建（联网：clone 上游 + 编译）
├── build_download_model.sh   # 从 ModelScope 下载 ASR 模型
├── config/
│   └── cmd_word.json         # 指令词配置（JSON 字符串数组）
├── include/
│   ├── alsa_audio.h          # ALSA 采集接口
│   ├── sense_engine.h        # 识别引擎接口与数据结构
│   └── tts_engine.h          # TTS 引擎接口
├── src/
│   ├── main.cpp              # 程序入口：参数解析、模式分发、回调接驳
│   ├── alsa_audio.cpp        # ALSA 采集封装
│   ├── sense_engine.cpp      # VAD + 识别 + 唤醒词 + 指令词（核心）
│   └── tts_engine.cpp        # TTS 合成 + 播放（espeak-ng / Piper）
├── third_party/
│   └── SenseVoice.cpp/       # 编译时自动 clone 的上游开源版（含 ggml 子模块）
├── test_zh.wav               # 离线测试音频（5.6s）
└── d4dfdfbc-test.wav         # 离线测试音频（2.0s，含「打开灯」指令）
```

---

## 3. 架构与数据流

### 3.1 总体流程

```
            ┌──────────────────────────────────────────────────────────┐
            │                        main.cpp                          │
            │                                                          │
  麦克风  ──┤ AlsaAudio::Read ──► SenseEngine::SendData (入队)          │
  (实时)    │                         │                                │
            │                         ▼                                │
  文件  ────┤ ReadWavToMono16k ─► SenseEngine::ProcessFile (同步)      │
  (离线)    │                         │                                │
            │              ┌──────────┴───────────┐                   │
            │              ▼                      ▼                   │
            │      ProcessLoop / FeedFrame   RunSenseVoice            │
            │      (VAD 断句 + 缓冲)         (ggml 推理)               │
            │              │                      │                   │
            │              └──────► on_asr(text) ─┘                   │
            │                          │                              │
            │                          ▼                              │
            │                  on_cmd(cmd / NONE)                     │
            │                          │                              │
            │              ┌───────────┴───────────┐                  │
            │              ▼                       ▼                  │
            │        printf [ASR]/[CMD]      tts->Speak(text)         │
            │                                      │                  │
            │                                      ▼                  │
            │                            TtsEngine::Worker            │
            │                      (后台线程：合成→播放)              │
            └──────────────────────────────────────────────────────────┘
```

### 3.2 两种运行模式

| 模式 | 触发 | 数据来源 | 处理模型 | 适用 |
|---|---|---|---|---|
| 实时麦克风 | 不传 `--file` | `AlsaAudio` 异步采集 | 生产者/消费者线程队列 | 真实拾音 |
| 离线文件 | 传 `--file <wav>` | `ReadWavToMono16k` 读全文件 | 同步 `ProcessFile` | 无麦/可复现测试 |

两者最终都汇入 `SenseEngine::FeedFrame()` ，共享同一套 VAD + 识别 + 匹配逻辑。

---

## 4. 模块实现说明

### 4.1 `AlsaAudio`（采集层）

文件：`include/alsa_audio.h` / `src/alsa_audio.cpp`

对 ALSA `snd_pcm_*` 接口的薄封装，等效原仓库 `src/utils/alsa_device.cpp`。

- `Open(device, rate, channels, period_size=512, nperiods=4)`：
  以 `SND_PCM_STREAM_CAPTURE` + `SND_PCM_ACCESS_RW_INTERLEAVED` + `SND_PCM_FORMAT_S16_LE` 配置硬件参数，
  采样率 `rate_near` 协商（`default 16000`），声道数 1 或 2。
- `Read(out, frames)`：每帧读取 `512` 个 period，自动 `snd_pcm_recover` 处理 XRUN。
  返回的是 **交织的原始 int16**（双声道时 `[L,R,L,R,...]` 顺序不变），**降混留给上层**。
- `Close()`：`snd_pcm_close`，析构自动调用。

> 注意：原始 int16 在上层以 `pending.size() > 1600` 粗略判断是否为双声道再降混
> （`sense_engine.cpp:188`）。这是 `main.cpp` 默认 `channels=2` 的约定产物。

### 4.2 `SenseEngine`（识别核心）

文件：`include/sense_engine.h` / `src/sense_engine.cpp`

等价原仓库 `speech_engine`，是工程的核心。采用 **生产者-消费者 + 后台推理线程** 模型。

#### 4.2.1 数据结构

```cpp
struct EngineConfig {
  std::string model_path;    // sense-voice-small-fp16.gguf
  std::string config_path;   // cmd_word.json 所在目录
  std::string language = "zh";
  std::string wakeword = "你好";
  int sample_rate = 16000;
  int n_threads = 4;
  bool enable_itn = true;    // 逆文本归一化，与原仓库一致
};

struct EngineCallbacks {
  std::function<void(const std::string&)> on_asr;  // 识别结果
  std::function<void(const std::string&)> on_cmd;  // 命中指令 / "NONE"
};
```

`Impl`（Pimpl 模式，避免暴露大量 ggml 头）：

```cpp
struct SenseEngine::Impl {
  struct sense_voice_context* ctx = nullptr;
  struct sense_voice_full_params params;
  bool ok = false;
};
```

#### 4.2.2 初始化 `Init()`

1. 加载 `cmd_word.json`（见 4.2.5）；
2. `sense_voice_context_default_params()` 关闭 GPU/FlashAttn，启用 ITN；
3. `sense_voice_small_init_from_file_with_params()` 加载 GGUF 模型（CPU，约 470MB）；
4. 设置 `language_id` 与全量推理参数 `sense_voice_full_default_params(SENSE_VOICE_SAMPLING_GREEDY)`：
   `no_timestamps=true`、`single_segment=true`、`print_*=false`；
5. 启动 `proc_thread_ = ProcessLoop()` 消费者线程。

#### 4.2.3 VAD 断句（能量 + 过零率）

复刻自上游 `sense-voice-frontend.h`，但用更轻量的规则替代 Silero-VAD，

```cpp
bool EnergyZcrVad(frame, energy_th, zcr_th) {
  energy = mean(frame[i]^2);
  zcr    = count(sign changes) / frame.size();
  return energy > energy_th || zcr_ratio > zcr_th;
}
```

`FeedFrame()` 的状态机（实际阈值见代码，做了抗误断调优）：

| 条件 | 动作 |
|---|---|
| 静音中有语音，且未触发 | 置 `vad_triggered_=true`，清空缓冲，开始累积 `vad_buf_` |
| 已触发 + 语音 | 续累积，重置 `silence_ms_=0` |
| 已触发 + 静音 | 累积静音时长进 `vad_buf_`；当 `silence_ms_ > 1200`（约 1.2s 静音）触发断句 |
| 断句后片段 `< 400ms` | 视为气口/噪声，丢弃（`ResetVad`） |
| 断句后片段 `≥ 400ms` | 调 `RunSenseVoice` 识别 → 触发 `on_asr` / `on_cmd` → `ResetVad` |

> 调优要点：能量阈值从默认 0.01 降到 0.004 以减少中间误断；过零率阈值从 0.2 降到 0.1 降低灵敏度；
> 静音断句阈值取 1200ms 以兼容「你好，打开灯」这类连说场景，避免被切开。

#### 4.2.4 识别与文本提取

`RunSenseVoice()`：

1. 把 `float` 音频转 `std::vector<double>`（上游接口要求）；
2. `sense_voice_full_parallel(ctx, params, samples, n, 1)` 推理；
3. 从 `ctx->state->ids` 恢复文本 —— 自 `i=4` 起逐 token 拼接，`id` 与上一项相同则跳过（去重）；
4. **唤醒词剥离**：若 `raw` 以 `wakeword` 开头，则 `substr` 去掉前缀。
   例：模型输出 `你好，打开灯。` → 剥离后回调 `，打开灯。`。
   （这就是 README 中「你好没有识别」问题的根因：唤醒词是**预期被剥离**的，并非漏识。）

#### 4.2.5 指令词匹配

`cmd_word.json` 为简单 JSON 字符串数组：

```json
{ "cmd_word": ["打开", "关闭", "前进", "后退", "停止", "开始"] }
```

`LoadCmdWords()` 用极简手写解析（不支持转义，仅够本配置用）。匹配为**子串查找**：
取第一个在识别文本中 `find != npos` 的词作为命中结果，否则回调 `"NONE"`。
匹配优先级即数组顺序。

#### 4.2.6 实时 vs 离线

- **实时**（`ProcessLoop`）：主线程 `SendData` 入队 → 消费者线程取帧 → 2 声道降混 → `FeedFrame`。
  队列上限 `kMaxQueueSize=256`（放宽松以避免离线模式下丢帧）。
- **离线**（`ProcessFile`）：以 `512` 样本为块同步喂入 `FeedFrame`，末尾补 64×512 个零（约 2s 静音）
  强制触发末次断句，确保文件尾语音被识别。

#### 4.2.7 释放

上游只提供 `sense_voice_free_state(ctx->state)`，**无** `sense_voice_free`。
`Stop()` 中先停线程，再 `sense_voice_free_state(impl_->ctx->state)` + `delete ctx`。

### 4.3 `TtsEngine`（语音合成播报）

文件：`include/tts_engine.h` / `src/tts_engine.cpp`

参考 `hobot_tts`「文本 → PCM → 播放」思路，但**不依赖地平线 `wetts`**（aarch64 闭源，PC 不可用），
改用 PC 可运行的 **espeak-ng**（默认）/ **Piper**（高质量，需额外模型）。

#### 4.3.1 设计

- **双后端**：`engine = "espeak" | "piper"`；
- **合成**：子进程调用 `espeak-ng` / `piper` 可执行，输出临时 WAV（`/tmp/pc_tts_XXXXXX.wav`，`mkstemps`）；
- **播放**：PulseAudio `paplay`（WSLg 下可用），失败回退 `aplay`；
- **串行消费**：一个后台 `Worker` 线程串行处理文本队列，避免并发抢占声卡。

#### 4.3.2 接口

```cpp
class TtsEngine {
  bool Start();                 // 启动后台播放线程
  void Speak(const std::string&); // 入队（线程安全）
  void Drain();                 // 阻塞等待队列播完
  void Stop();                  // 停止线程（等待当前项结束）
};
```

#### 4.3.3 合成命令构造

- **espeak-ng**（默认）：

  ```bash
  ESPEAK_DATA_PATH=<piper_dir>/espeak-ng-data \
  [LD_LIBRARY_PATH=<piper_dir>] \
  espeak-ng -v <voice> -s <rate> --stdout '<text>' > <wav>
  ```

  关键：用 `--stdout` 重定向到文件，**而非 `-w`**。`-w` 会初始化音频播放设备，
  在 WSLg/PulseAudio 下会挂起；`--stdout` 只做文本合成、不碰声卡，播放交给 `paplay`。

- **Piper**（高质量）：

  ```bash
  echo '<text>' | [LD_LIBRARY_PATH=<piper_dir>] \
  piper --model <model.onnx> --espeak_data <piper_dir>/espeak-ng-data \
        --output_file <wav>
  ```

  `--espeak_data` 指向 Piper 自带的中文音素数据目录。

- `LD_LIBRARY_PATH` 接 `piper_dir`：因 `piper_dir` 自带 `libespeak-ng.so` / `libonnxruntime.so`，
  必须注入以便 `piper`/`espeak-ng` 找到运行时依赖。`ShellEscape()` 对文本做单引号转义，
  防止特殊字符破坏命令行。

#### 4.3.4 与主程序接驳

`main.cpp` 中：

```cpp
tts->Speak(text);                              // [ASR] 回调：播报完整识别文本
if (!cmd.empty() && cmd != "NONE")
  tts->Speak("收到指令：" + cmd);              // [CMD] 回调：命中指令时播报
```

退出前 `tts->Drain(); tts->Stop();` 确保队列播完再退出。

### 4.4 `main.cpp`（入口与编排）

职责：

1. `ParseArgs` 解析全部 CLI 参数（见 README 参数表）；
2. 构造 `EngineConfig` 并 `SenseEngine::Init`（注入 `on_asr` / `on_cmd` 回调）；
3. 若 `--speak`，构造并 `Start` 一个 `TtsEngine`；
4. 模式分发：
   - `--file` 存在 → `ReadWavToMono16k` + `ProcessFile`（同步）；
   - 否则 → `AlsaAudio::Open` + 循环 `Read` + `SendData`（异步）；
5. `SIGINT`/`SIGTERM` → `g_stop=1` 优雅退出；
6. `engine.Stop()` + `tts->Drain()/Stop()` 收尾。

`ReadWavToMono16k()`：手写的 WAV（RIFF）解析器，仅支持 16-bit PCM；
自动把任意声道数转单声道、线性插值重采样到 16k，返回 `int16` 序列。
chunk 遍历容错（跳过 `fmt` 扩展字节、未知 chunk）。

---

## 5. 构建系统

文件：`CMakeLists.txt`

要点：

- `find_package(ALSA REQUIRED)`；
- **上游接入**：优先 `SENSEVOICE_SRC_DIR`（CMake `-D` 或环境变量）> `third_party/SenseVoice.cpp` > 自动 `git clone --recursive`；
- `add_subdirectory(${SENSEVOICE_SRC_DIR} ... EXCLUDE_FROM_ALL)`，
  并强制 `SENSE_VOICE_BUILD_EXAMPLES=OFF` / `SENSE_VOICE_BUILD_TESTS=OFF` 以加速；
- 可执行目标 `sensevoice_pc_demo` 链接 `sense-voice-core` + `ALSA` + `pthread` + `dl` + `m`；
- 包含目录需覆盖上游 `sense-voice/csrc` 与 `ggml` 的 include（含 `sense-voice.h`）。

> 构建前**必须**确保上游 `ggml` 子模块已拉全，否则 `sense-voice-core` 编译失败。
> 详见 README「踩坑与要点」。

---

## 6. 配置说明

### 6.1 `cmd_word.json`

```json
{ "cmd_word": ["打开", "关闭", "前进", "后退", "停止", "开始"] }
```

- 仅支持扁平字符串数组（手写解析，不支持转义/嵌套）；
- 匹配为子串命中，命中第一个即返回；
- 可自由增删词条，无需重编译（运行时读取）。

### 6.2 关键阈值（如需调优）

集中在 `sense_engine.cpp`：

| 参数 | 位置 | 含义 | 当前值 |
|---|---|---|---|
| `energy_th` | `FeedFrame` | VAD 能量阈值（越低越灵敏） | `0.004` |
| `zcr_th` | `FeedFrame` | VAD 过零率阈值 | `0.10` |
| 静音断句 | `FeedFrame` | 触发断句的静音时长 | `> 1200ms` |
| 最短片段 | `FeedFrame` | 丢弃过短语音 | `< 400ms` |
| 队列上限 | 类成员 | 实时模式防丢帧 | `256` |

### 6.3 TTS 资源

- **espeak-ng**：可用 `hobot_tts` 自带的运行时（`pc_tts_deps/piper/` 下含 `espeak-ng` + `espeak-ng-data` + `.so`），或系统 `apt install espeak-ng`。
- **Piper 模型**：ModelScope `Trelis/piper-zh-cn-huayan-medium`（约 63MB，`model.onnx` + `model.onnx.json` 同名同目录）。

---

## 7. 已知限制与后续方向

1. **无 ROS2**：仅打印文本，无话题发布。如需接入 ROS2，可在 `main.cpp` 的回调里加 `rclcpp` 发布。
2. **VAD 较简单**：能量 + 过零率断句精度逊于 Silero-VAD。如需更准，可集成上游 `silero_vad.onnx`（onnxruntime）。
3. **子进程调用 TTS**：`std::system` 调用 `espeak-ng`/`piper` 有进程启动开销，高频播报可考虑直接链接其库 API。
4. **WSLg 播放依赖 RDP**：WSL2 下麦克风（`RDPSource`，即 Windows 物理麦克风桥接）已实测可用，本地终端即可拾音识别；但扬声器（`RDPSink`）仅在 RDP 远程桌面会话激活时才有输出，纯本地终端下为 `SUSPENDED`，听不到 TTS 播报（识别/合成仍正常）。详见 `README.md` 第 5.1 节。
5. **指令词为子串匹配**：无法处理歧义或多词组合，复杂场景需改成正则/语义匹配。
6. **`RunSenseVoice` 用 `sense_voice_full_parallel`**：大模型时该并行接口可提速；小模型用 `sense_voice_full` 亦可。

---

## 8. 验证状态（2026-08-17 ~ 08-18）

| 验证项 | 环境 | 结果 |
|---|---|---|
| x86_64 编译（上游 + demo） | WSL2 Ubuntu 22.04 | ✅ |
| ASR 模型加载（448MB gguf，50 层 encoder） | WSL2 | ✅ |
| 指令词解析（8 条） | — | ✅ |
| 离线文件全链路（识别 + 指令命中） | WSL2 | ✅（见 README 示例） |
| TTS espeak-ng 播放 | WSL2 + WSLg/paplay | ✅ |
| TTS Piper 播放（ModelScope 模型） | WSL2 + WSLg/paplay | ✅ |
| 实时麦克风拾音（RDPSource） | WSL2 + Windows 麦克风 | ✅（需 RDP 连入才能听到播报） |
| 程序优雅退出 | — | ✅ |
| 纯标点过滤不误触发 TTS | WSL2 | ✅（已修复 `IsContentLess`） |
