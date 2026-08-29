#!/usr/bin/env bash
# 在 WSL 交互终端里运行：麦克风说话 -> SenseVoice 识别 -> TTS 语音播报
#
# 用法（在 WSL 终端执行）：
#   bash test_live.sh              # espeak-ng 后端（默认，开箱即用）
#   bash test_live.sh piper        # Piper 高质量后端（需 model.onnx）
#   bash test_live.sh espeak 30    # 指定后端 + 运行秒数（仅用于定时自动退出）
#
# 退出：前台运行时按 Ctrl+C 优雅退出（会等 TTS 队列播完）。
#
# 说明：
#   - 麦克风走 WSLg 默认源（即 Windows 物理麦克风，WSLg 暴露为 RDPSource）。
#     若 WSL 里 pactl get-default-source 不是你的麦克风，本脚本会用 --device default
#     仍尝试；如需指定设备，改 DEVICE 变量或加 --device 参数。
#   - 依赖：build/bin/sensevoice_pc_demo 已构建；ASR 模型已下载；
#     TTS 运行时在 /mnt/d/.../hobot_tts/pc_tts_deps/piper（Windows 侧，WSL 可访问）。

set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ---- 路径自动探测（优先 WSL 家目录副本，其次脚本所在目录）----
if [ -x "$HOME/pc_demo/build/bin/sensevoice_pc_demo" ]; then
  BASE="$HOME/pc_demo"
elif [ -x "$SCRIPT_DIR/build/bin/sensevoice_pc_demo" ]; then
  BASE="$SCRIPT_DIR"
else
  echo "ERROR: 找不到 build/bin/sensevoice_pc_demo（请先构建）"
  echo "  搜索过: $HOME/pc_demo 和 $SCRIPT_DIR"
  exit 1
fi

BIN="$BASE/build/bin/sensevoice_pc_demo"
CFG="$BASE/config/cmd_word.json"

# ASR 模型：优先 WSL 家目录上游，其次 /mnt/d 下
if [ -f "$HOME/SenseVoice.cpp/SenseVoice.cpp/models/sense-voice-small-fp16.gguf" ]; then
  MODEL="$HOME/SenseVoice.cpp/SenseVoice.cpp/models/sense-voice-small-fp16.gguf"
elif [ -f "/mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf" ]; then
  MODEL="/mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf"
else
  echo "ERROR: 找不到 ASR 模型 sense-voice-small-fp16.gguf"
  exit 1
fi

# TTS 运行时（Windows 侧 hobot_tts/pc_tts_deps，WSL 经 /mnt/d 访问）
PIPER_DIR="/mnt/d/work/github/hobot_tts/pc_tts_deps/piper"
PIPER_MODEL="/mnt/d/work/github/hobot_tts/pc_tts_deps/model.onnx"

ENGINE="${1:-espeak}"
AUTO_SECONDS="${2:-}"

echo "=== 麦克风源（应为你的 Windows 麦克风，WSLg 暴露为 RDPSource）==="
pactl list short sources 2>/dev/null
DEF="$(pactl get-default-source 2>/dev/null)"
echo "默认源: $DEF"
echo

if [ ! -d "$PIPER_DIR" ]; then
  echo "WARN: TTS 运行时目录不存在: $PIPER_DIR（将无法播放）"
fi

# ---- 组装 TTS 参数 ----
TTS_ARGS="--speak --tts-engine $ENGINE --piper-dir $PIPER_DIR"
if [ "$ENGINE" = "piper" ]; then
  if [ ! -f "$PIPER_MODEL" ]; then echo "ERROR: Piper 模型不存在: $PIPER_MODEL"; exit 1; fi
  TTS_ARGS="$TTS_ARGS --piper-model $PIPER_MODEL --tts-voice zh"
else
  TTS_ARGS="$TTS_ARGS --tts-voice zh --tts-rate 175"
fi

echo "=== 启动 demo（实时麦克风，后端=$ENGINE）==="
echo "请对麦克风说：你好，打开灯"
echo "识别后会通过 TTS 播报（[ASR] 文本 / [CMD] 命中指令）。"
echo "退出：Ctrl+C"
echo

"$BIN" --model "$MODEL" --device default --wakeword 你好 --lang zh \
       --cmd-config "$CFG" --channels 1 $TTS_ARGS &
PID=$!

if [ -n "$AUTO_SECONDS" ]; then
  # 仅定时自动退出模式（无交互终端时）
  echo "(自动运行 $AUTO_SECONDS 秒后退出, pid=$PID)"
  sleep "$AUTO_SECONDS"
  kill -INT "$PID" 2>/dev/null
  wait "$PID"
  echo "=== 测试结束 ==="
else
  # 交互模式：等待用户 Ctrl+C
  wait "$PID"
fi
