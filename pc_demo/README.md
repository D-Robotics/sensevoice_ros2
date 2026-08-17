# sensevoice_ros2 —— PC 版独立 Demo

在 **Ubuntu 22.04 x86_64 PC** 上，用开源 `lovemefan/SenseVoice.cpp` 复刻本仓库 `speech_engine` 的算法逻辑，
实现「麦克风采集 → VAD 断句 → SenseVoice 识别 → 唤醒词剥离 → 指令词匹配」的完整流程。
不依赖 ROS2、不依赖地平线闭源库（`libsense-voice-core`）。

## 特性

- 与 `sensevoice_ros2` 相同的行为：`lib/speech_engine` 里唤醒词剥离 + `config/cmd_word.json` 指令词匹配
- 实时麦克风（ALSA）采集 16kHz 单声道
- 能量+过零率 VAD（`lib/vad_energy_zcr`，复刻自 `sense-voice-frontend.h`）断句，静音超时自动识别
- 纯 CPU 推理（ggml），无需 GPU

## 目录结构

```
pc_demo/
├── README.md
├── CMakeLists.txt
├── build.sh
├── src/
│   ├── main.cpp          # 等效 hb_audio_capture：采集 + VAD 循环 + 打印结果
│   ├── alsa_audio.cpp    # ALSA 采集封装（等效 src/utils/alsa_device.cpp）
│   └── sense_engine.cpp  # 等效 speech_engine：特征、唤醒词、指令词、回退 VAD
├── include/
│   ├── alsa_audio.h
│   └── sense_engine.h
├── config/
│   └── cmd_word.json     # 指令词配置（等效原仓库 config/cmd_word.json）
└── third_party/          # 编译时自动 git clone 的 SenseVoice.cpp（上游开源版）
```

## 依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake git libasound2-dev
# 可选：SDL2（SenseVoice.cpp 自带 stream 示例用，本 demo 不用，可不装）
# sudo apt install -y libsdl2-dev
```

## 编译

上游 `SenseVoice.cpp` 含 `ggml` 子模块，**必须先拉全子模块**。

### 方式 A：源码已就绪，用 SENSEVOICE_SRC_DIR 指定（推荐，离线可用）

```bash
# 1. 准备好上游源码（含 ggml 子模块）
git clone --recurse-submodules --depth 1 \
  https://github.com/lovemefan/SenseVoice.cpp.git /path/to/SenseVoice.cpp
# 若子模块没拉全，补拉：
#   cd /path/to/SenseVoice.cpp && git submodule update --init --recursive

# 2. 下载模型（仅 ASR 模型，约 448MB）
#   https://modelscope.cn/models/lovemefan/SenseVoiceGGUF/resolve/master/sense-voice-small-fp16.gguf
#   放到 /path/to/SenseVoice.cpp/models/ 下

# 3. 构建（指定上游路径，避免重新 clone）
cd pc_demo
export SENSEVOICE_SRC_DIR=/path/to/SenseVoice.cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSENSEVOICE_SRC_DIR="$SENSEVOICE_SRC_DIR"
cmake --build build -j
```

### 方式 B：全自动（联网时）

```bash
cd pc_demo
./build.sh     # 自动 git clone 上游（含子模块）并构建，产物在 build/
./build_download_model.sh   # 下载模型到 third_party/SenseVoice.cpp/models/
```

> CMake 查找上游的优先级：`-DSENSEVOICE_SRC_DIR=` / 环境变量 `SENSEVOICE_SRC_DIR` > `third_party/SenseVoice.cpp` > 自动 clone。

模型位置（取决于方式）：

- 方式 A：`$SENSEVOICE_SRC_DIR/models/sense-voice-small-fp16.gguf`
- 方式 B：`third_party/SenseVoice.cpp/models/sense-voice-small-fp16.gguf`

> 注：本 demo 使用能量+过零率 VAD，**不依赖 `silero_vad.onnx`**；该文件仅作上游保留，无需下载。

## 运行

```bash
cd pc_demo
./build/bin/sensevoice_pc_demo \
  --model /path/to/SenseVoice.cpp/models/sense-voice-small-fp16.gguf \
  --device plughw:0,0 \
  --wakeword 你好 \
  --lang zh \
  --cmd-config config/cmd_word.json
```

参数：

| 参数 | 默认 | 说明 |
|---|---|---|
| `--model` | 自动查找 `third_party/.../sense-voice-small-fp16.gguf` | ASR 模型路径 |
| `--device` | `plughw:0,0` | ALSA 麦克风设备 |
| `--file` | 空（不启用） | 离线 WAV 文件识别（跳过麦克风，用于无麦克风/可复现测试） |
| `--wakeword` | `你好` | 唤醒词（会从识别文本中剥离） |
| `--lang` | `zh` | 语言 |
| `--cmd-config` | `config/cmd_word.json` | 指令词配置 |
| `--threads` | `4` | 推理线程数 |

离线文件模式示例：

```bash
./build/bin/sensevoice_pc_demo \
  --model /path/to/sense-voice-small-fp16.gguf \
  --file test_zh.wav \
  --wakeword 你好 --lang zh --cmd-config config/cmd_word.json
```

> WAV 支持：16-bit PCM，自动重采样/转单声道到 16kHz；文件尾自动补静音以触发末次断句识别。
> 没有麦克风时，也可用 ALSA `plug` 插件喂 WAV，或用 `arecord -f cd -t wav | ./... ` 管道方式（demo 未支持，可自行扩展）。

## 使用流程（同原仓库）

1. 对麦克风说唤醒词「你好」
2. 继续说指令，如「你好，打开灯」
3. 控制台打印：`ASR: 打开灯`，`CMD: 打开`
4. 未命中指令词时打印 `CMD: NONE`

## 与本仓库的对应关系

| 本仓库（RDK） | PC demo |
|---|---|
| `src/hb_audio_capture.cpp` | `src/main.cpp`（采集+VAD 循环） |
| `src/speech_engine.cpp` | `src/sense_engine.cpp`（算法迁移） |
| `src/utils/alsa_device.cpp` | `src/alsa_audio.cpp` |
| `include/sensevoice/lib/*.a/.so`（闭源） | `third_party/SenseVoice.cpp`（开源上游） |
| `config/cmd_word.json` | `config/cmd_word.json` |

## 限制

- 无 ROS 话题，只打印文本（如需 ROS2 可在此基础上加 `rclcpp`）
- 使用能量+过零率 VAD（轻量、无需 onnx），断句效果略逊于 Silero-VAD；如需更准可集成上游 `silero_vad.onnx`（通过 onnxruntime）
- `speech_engine` 的 `sense_voice_full` 为整段识别；SenseVoice.cpp 也支持 `sense_voice_full_parallel`，大模型时可提升速度

## WSL2 验证记录（2026-08-17）

在 **Windows WSL2 + Ubuntu 22.04 x86_64** 上完成编译与运行验证。

### 环境

- WSL 分发：`Ubuntu-22.04`
- 工具链：`cmake / gcc / g++ / git / make / wget`（均满足）
- 依赖：`libasound2-dev`（ALSA 开发库）
- 上游源码：`/home/shulu/SenseVoice.cpp`（Windows 侧 `D:\work\github\SenseVoice.cpp` 复制而来，含 `ggml` 子模块）
- 模型：`SenseVoice.cpp/models/sense-voice-small-fp16.gguf`（448MB，来自 ModelScope）

### 踩坑与要点

1. **子模块必须拉全**：`SenseVoice.cpp` 依赖 `sense-voice/csrc/third-party/ggml` 子模块。从 Windows 复制时若 ggml 是空占位，需在 WSL 内单独 clone 或重新 `cp -r` 整个已就绪的 Windows 目录。
2. **避免 `/mnt/d` 直接编译**：WSL 挂载的 Windows 盘大小写不敏感、git 缓存易异常，建议把 `SenseVoice.cpp` 和 `pc_demo` 复制到 `~/` 下再编译。
3. **`SENSEVOICE_SRC_DIR` 变量**：CMake 优先读环境变量 / `-D` 传入的上游路径。若 `pc_demo/third_party/SenseVoice.cpp` 残留了旧副本，会误用默认路径导致 ggml 找不到——需删除残留副本。
4. **释放函数**：上游只导出 `sense_voice_free_state(ctx->state)`，**没有** `sense_voice_free`。`sense_engine.cpp` 的 `Stop()` 用 `sense_voice_free_state(impl_->ctx->state)` + `delete ctx`。

### 验证结果

```text
$ export SENSEVOICE_SRC_DIR=/home/shulu/SenseVoice.cpp
$ cd ~/pc_demo && rm -rf build && mkdir build && cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Release        # CMAKE_EXIT=0
$ make -j$(nproc)                             # MAKE_EXIT=0，生成 build/bin/sensevoice_pc_demo

$ ./build/bin/sensevoice_pc_demo \
    --model /mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf \
    --device null --cmd-config config/cmd_word.json

sense_voice_model_load: n_vocab = 25055
sense_voice_model_load: n_encoder_layers = 50
sense_voice_model_load: CPU total size = 469.87 MB
sense_voice_init_state: ...
[sense_engine] init ok: model=... lang=zh wakeword=你好 cmds=7
[main] mic opened: null rate=16000 ch=2, say wake word "你好" then command...
[main] bye
```

| 验证项 | 结果 |
|--------|------|
| WSL2 x86_64 编译（上游库 + demo） | ✅ |
| 模型加载（448MB gguf，50 层 encoder） | ✅ |
| 引擎初始化 + 指令词配置（7 条）解析 | ✅ |
| 程序正常退出（无崩溃） | ✅ |
| 实时麦克风识别 | ⚠️ 需原生 Ubuntu + 真实麦克风（WSL2 音频不通） |

> 实时识别建议在**原生 Ubuntu 22.04 物理机**上运行，将 `--device` 指向真实 ALSA 设备（如 `plughw:0,0`）。可用 `arecord -l` 查看设备名。
