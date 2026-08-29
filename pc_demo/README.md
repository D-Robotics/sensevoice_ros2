# sensevoice_ros2 —— PC 版独立 Demo（操作手册）

在 **Ubuntu 22.04 x86_64 PC / WSL2** 上，用开源 `lovemefan/SenseVoice.cpp` 复刻本仓库 `speech_engine`
的算法逻辑，实现「麦克风采集 → VAD 断句 → SenseVoice 识别 → 唤醒词剥离 → 指令词匹配 → TTS 播报」
的完整流程。不依赖 ROS2、不依赖地平线闭源库（`libsense-voice-core`）。

> 本文是**操作手册**：怎么编译、怎么弄到 WSL、怎么跑测试、怎么看输出。
> 设计细节（模块实现、数据流、阈值）见 `docs/technical.md`。

---

## 0. 一句话上手（WSL 已就绪时）

如果你的环境已经配好（上游源码 + 模型 + WSL 副本都在），最后一步只是：

```bash
# 在 WSL 终端里
cd ~/pc_demo
bash test_live.sh piper      # 麦克风说话 → 识别 → Piper 语音播报
```

说「你好，打开灯」，应看到 `[ASR] ，打开灯。` / `[CMD] 打开` 并听到播报。
下面是从零开始的完整步骤。

---

## 1. 环境依赖

```bash
# Ubuntu / WSL2 里执行
sudo apt update
sudo apt install -y build-essential cmake git libasound2-dev
# TTS 运行时（espeak-ng / Piper）由 hobot_tts 的 pc_tts_deps 提供，见第 4 节
```

工具链要求：`cmake ≥ 3.16`、`gcc/g++` 支持 C++17。

---

## 2. 编译

上游 `SenseVoice.cpp` 依赖 `ggml` 子模块，**子模块必须拉全**，否则编译失败。

### 方式 A：指定已存在的上游源码（推荐，离线可用）

```bash
# 1) 准备上游（含 ggml 子模块）
git clone --recurse-submodules --depth 1 \
  https://github.com/lovemefan/SenseVoice.cpp.git /path/to/SenseVoice.cpp
# 若子模块没拉全：cd /path/to/SenseVoice.cpp && git submodule update --init --recursive

# 2) 放 ASR 模型（约 450MB）到上游的 models/ 下
#    https://modelscope.cn/models/lovemefan/SenseVoiceGGUF/resolve/master/sense-voice-small-fp16.gguf
#    → /path/to/SenseVoice.cpp/models/sense-voice-small-fp16.gguf

# 3) 构建
cd pc_demo
export SENSEVOICE_SRC_DIR=/path/to/SenseVoice.cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 方式 B：全自动（联网）

```bash
cd pc_demo
./build.sh                    # 自动 clone 上游(含子模块) + 编译
./build_download_model.sh     # 下载模型到 third_party/SenseVoice.cpp/models/
```

产物：`pc_demo/build/bin/sensevoice_pc_demo`

> CMake 查找上游优先级：`-DSENSEVOICE_SRC_DIR=` / 环境变量 `SENSEVOICE_SRC_DIR`
> > `third_party/SenseVoice.cpp` > 自动 clone。

---

## 3. 把程序弄到 WSL 上

**重要**：WSL 通过 `/mnt/d/...` 访问 Windows 盘，但在该盘上**编译不稳定**（大小写不敏感、
inode/缓存异常），且我们的实际运行副本在 WSL 家目录 `~/pc_demo`。因此采用「Windows 侧编辑源码 +
脚本同步到 WSL 副本 + 在 WSL 内编译」的分工。

### 3.1 首次初始化 WSL 副本

```bash
# 在 WSL 终端执行（只需一次）
# 假设 Windows 侧仓库在 /mnt/d/work/github/sensevoice_ros2
mkdir -p ~/pc_demo
cp -r /mnt/d/work/github/sensevoice_ros2/pc_demo/src      ~/pc_demo/
cp -r /mnt/d/work/github/sensevoice_ros2/pc_demo/include  ~/pc_demo/
cp -r /mnt/d/work/github/sensevoice_ros2/pc_demo/config   ~/pc_demo/
cp    /mnt/d/work/github/sensevoice_ros2/pc_demo/CMakeLists.txt ~/pc_demo/
cp    /mnt/d/work/github/sensevoice_ros2/pc_demo/test_live.sh    ~/pc_demo/

# 上游源码也放到 WSL 家目录（不要放 /mnt/d 下编译）
# 例：把 Windows 的 D:\work\github\SenseVoice.cpp 复制进来
cp -r /mnt/d/work/github/SenseVoice.cpp ~/SenseVoice.cpp
```

### 3.2 改完代码后：同步 + 重新编译

项目根已提供一键同步脚本 `pc_demo/_sync_and_build.sh`，它把 Windows 侧改动的
`src/`、`include/`、`config/`、`CMakeLists.txt`、`test_live.sh` 复制到 `~/pc_demo` 并重新 `cmake --build`：

```bash
# 在 WSL 终端执行（改了代码后跑一次）
bash /mnt/d/work/github/sensevoice_ros2/pc_demo/_sync_and_build.sh
```

脚本内部等价：
```bash
SRC=/mnt/d/work/github/sensevoice_ros2/pc_demo
DST=~/pc_demo
cp $SRC/src/tts_engine.cpp   $DST/src/
cp $SRC/include/tts_engine.h $DST/include/
cp $SRC/config/cmd_word.json $DST/config/
cp $SRC/CMakeLists.txt       $DST/
cp $SRC/test_live.sh         $DST/
cd $DST/build && cmake --build . -j$(nproc)
```

> 注意：`_sync_and_build.sh` 是以下划线开头的**开发辅助脚本**，不同步到仓库正式文档，
> 仅用于本地「Windows 编辑 → WSL 构建」的快速回路。

---

## 4. TTS 运行时（espeak-ng / Piper）

TTS 依赖 `hobot_tts` 仓库里的 `pc_tts_deps/piper`（含 `espeak-ng`、`espeak-ng-data`、
`libespeak-ng.so`、`libonnxruntime.so`、`piper` 可执行）。本机预期路径：

```
/mnt/d/work/github/hobot_tts/pc_tts_deps/piper/        # 可执行 + 数据 + .so
/mnt/d/work/github/hobot_tts/pc_tts_deps/model.onnx    # Piper 中文模型(若用 piper 引擎)
```

若你机器上路径不同，编辑 `test_live.sh` 顶部的 `PIPER_DIR` / `PIPER_MODEL` 变量，
或运行 demo 时显式传 `--piper-dir` / `--piper-model`。

### 下载 Piper 中文模型（可选，仅 piper 引擎需要）

```bash
# 仓库：Trelis/piper-zh-cn-huayan-medium（中文女声，约 63MB）
# model.onnx 与 model.onnx.json 必须同名同目录，--piper-model 指向 .onnx
```

---

## 5. 运行测试

两种模式：**实时麦克风**（说话测试）和 **离线文件**（可复现测试）。

### 5.1 实时麦克风（推荐用 `test_live.sh`）

`test_live.sh` 已封装好路径探测、麦克风源打印、TTS 参数，直接在 WSL 交互终端跑：

```bash
cd ~/pc_demo
bash test_live.sh            # 默认 espeak-ng 后端
bash test_live.sh piper      # Piper 高质量后端（需 model.onnx）
bash test_live.sh espeak 30  # 限时 30 秒自动退出的无交互模式
```

脚本会：
1. 打印 `pactl list short sources` —— 确认麦克风源（WSLg 下是 `RDPSource`，即 Windows 物理麦克风桥接）；
2. 以前台进程启动 demo，按 `Ctrl+C` 优雅退出（等 TTS 队列播完）。

**测试话术**：先说唤醒词「你好」，再说指令，如「你好，打开灯」「你好，前进」。

> **播放前提（关键）**：WSLg 的扬声器设备 `RDPSink` **只在 RDP（远程桌面）会话激活时才有输出**。
> 也就是说：要通过 TTS 听到播报，你需要**用 Windows 远程桌面（RDP）连入这台 WSL**，
> 并在连接选项里勾选「音频重定向 / 在远程计算机上播放」。纯本地终端（无 RDP）下
> `RDPSink` 状态为 `SUSPENDED`，识别与合成照常工作，但**听不到声音**。
> 麦克风（`RDPSource`）则不受此限制，本地终端也能拾音。

### 5.2 离线文件模式（无麦 / 可复现）

用自带的测试音频验证「读取→VAD→识别→指令匹配→TTS」整条链路：

```bash
cd ~/pc_demo
./build/bin/sensevoice_pc_demo \
  --model /path/to/sense-voice-small-fp16.gguf \
  --file test_zh.wav \
  --wakeword 你好 --lang zh --cmd-config config/cmd_word.json \
  --speak --tts-engine piper \
  --piper-dir /mnt/d/work/github/hobot_tts/pc_tts_deps/piper \
  --piper-model /mnt/d/work/github/hobot_tts/pc_tts_deps/model.onnx
```

`test_zh.wav`（5.6s）内容不含指令词，预期 `[CMD] NONE`；
`d4dfdfbc-test.wav`（2.0s，含「打开灯」）预期 `[ASR] ，打开灯。` / `[CMD] 打开`。

### 5.3 常用参数

| 参数 | 默认 | 说明 |
|---|---|---|
| `--model` | 自动查找上游 `models/sense-voice-small-fp16.gguf` | ASR 模型路径 |
| `--device` | `default` | 麦克风设备（WSLg 用 `default` 即可） |
| `--file` | 空 | 离线 WAV 识别（绕过麦克风） |
| `--wakeword` | `你好` | 唤醒词（从识别文本中剥离） |
| `--lang` | `zh` | 语言 |
| `--cmd-config` | `config/cmd_word.json` | 指令词配置 |
| `--speak` | 关 | 启用 TTS 播报 |
| `--tts-engine` | `espeak` | `espeak` \| `piper` |
| `--piper-dir` | 空 | piper/espeak-ng 运行时目录 |
| `--piper-model` | 空 | Piper `.onnx` 模型（piper 引擎必填） |
| `--tts-voice` | `zh` | espeak-ng 发音人 |
| `--tts-rate` | `175` | espeak-ng 语速 |

---

## 6. 如何看测试信息（日志解读）

程序把信息打到 **stdout/stderr**，关键行如下：

```
[main] TTS enabled: engine=piper                       # TTS 已启用，后端类型
[sense_engine] init ok: ... cmds=9                     # 引擎初始化成功，指令词条数=9
[main] mic opened: default rate=16000 ch=1             # 麦克风已打开（实时模式）
[main] file mode: xxx.wav, N samples (Xs @16000)       # 离线文件模式载入

[ASR] ，打开灯。                                       # 识别结果（唤醒词「你好」已被剥离）
[CMD] 打开                                             # 命中指令词（NONE=未命中）

[2026-08-18 ...] [piper] Loaded voice ...              # Piper 加载模型
[2026-08-18 ...] [piper] Real-time factor: ... audio=1.22s   # 合成完成，audio=实际语音秒数
/tmp/pc_tts_XXXXXX.wav                                 # 本次合成出的临时 wav 路径
```

### 排错速查

| 现象 | 含义 / 处理 |
|---|---|
| `[ASR] 。` 反复出现、无内容 | 麦克风没拾到有效语音（安静或没说话），属正常噪声 |
| `[CMD] NONE` | 识别文本里没匹配到 `cmd_word.json` 中的词（子串匹配） |
| `RDPSink ... SUSPENDED` + 听不到播报 | 未连 RDP，无音频输出设备；用远程桌面连入 WSL 再试 |
| `piper synthesis failed` / `paplay: ...` | 检查 `PIPER_DIR` / `PIPER_MODEL` 路径、`LD_LIBRARY_PATH` |
| 编译报 `ggml` 找不到 | 上游子模块没拉全，回第 2 节补 `git submodule update` |
| 启动报找不到 `build/bin/sensevoice_pc_demo` | 没编译，或路径探测没命中 `~/pc_demo` / 脚本目录 |

### 想把日志存下来分析

```bash
# 实时模式限时跑、日志落盘（便于事后检查）
bash test_live.sh piper > /tmp/live.log 2>&1
# 另开终端实时跟踪
tail -f /tmp/live.log
```

---

## 7. 指令词配置

`config/cmd_word.json` 为扁平字符串数组，**运行时读取、改完无需重编译**：

```json
{ "cmd_word": ["打开", "关闭", "开灯", "关灯", "前进", "后退", "停止", "开始"] }
```

匹配规则：子串查找，取**第一个**在识别文本中出现（`.find != npos`）的词返回；都没有则回调 `NONE`。
增删词条直接编辑文件即可。

---

## 8. 已知限制

1. **无 ROS2**：只打印文本，不发布话题。
2. **WSLg 播放依赖 RDP**：扬声器 `RDPSink` 仅在远程桌面会话激活时有输出（见 5.1）。麦克风不受限。
3. **VAD 较简单**：能量 + 过零率断句，精度逊于 Silero-VAD。
4. **Piper 每次合成重载模型**：当前实现每次播报都 reload voice（约 0.6s），连续播报有累计延迟；功能正常，后续可优化为常驻。
5. **指令词为子串匹配**：无法处理歧义/多词组合。
6. **纯标点不播报**：识别出「。」等无语义结果时，`TtsEngine` 会过滤掉，不进入合成队列。

---

## 9. 验证状态（2026-08-18）

| 验证项 | 环境 | 结果 |
|---|---|---|
| x86_64 编译（上游 + demo） | WSL2 Ubuntu 22.04 | ✅ |
| ASR 模型加载（~470MB gguf，50 层 encoder） | WSL2 | ✅ |
| 指令词解析（9 条） | — | ✅ |
| 离线文件全链路（识别 + 指令命中） | WSL2 | ✅ |
| TTS espeak-ng 播放 | WSL2 + WSLg/paplay | ✅ |
| TTS Piper 播放 | WSL2 + WSLg/paplay | ✅ |
| 实时麦克风拾音（RDPSource） | WSL2 + Windows 麦克风 | ✅（需 RDP 连入才能听到播报） |
| 程序优雅退出 | — | ✅ |
| 纯标点过滤不误触发 TTS | WSL2 | ✅（已修复） |
