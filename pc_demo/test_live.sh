#!/usr/bin/env bash
# 本机控制台（非 RDP）一键实时识别测试
# 用法： bash ~/pc_demo/test_live.sh [秒数]
set +e
MODEL=/mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf
BIN=/home/shulu/pc_demo/build/bin/sensevoice_pc_demo
CFG=/home/shulu/pc_demo/config/cmd_word.json

echo "=== 当前 PulseAudio 默认输入源（应为你的物理麦克风）==="
pactl get-default-source
echo "=== 可用输入源列表 ==="
pactl list short sources
echo

if [ ! -x "$BIN" ]; then
  echo "ERROR: demo 未构建: $BIN"; exit 1
fi
if [ ! -f "$MODEL" ]; then
  echo "ERROR: 模型不存在: $MODEL"; exit 1
fi

echo "=== 启动 demo（--device default 走 WSLg/PulseAudio）==="
echo "请对麦克风说：你好，打开灯"
"$BIN" --model "$MODEL" --device default --wakeword 你好 --lang zh --cmd-config "$CFG" --channels 1 &
PID=$!
SLEEP=${1:-15}
echo "(demo pid=$PID, 运行 $SLEEP 秒后自动退出)"
sleep "$SLEEP"
kill -INT "$PID" 2>/dev/null
wait "$PID"
echo "=== 测试结束 ==="
