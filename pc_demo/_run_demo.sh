#!/usr/bin/env bash
set +e
MODEL=/mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf
BIN=~/pc_demo/build/bin/sensevoice_pc_demo
CFG=~/pc_demo/config/cmd_word.json

echo "=== starting demo with --device default (PulseAudio/WSLg) ==="
"$BIN" --model "$MODEL" --device default --wakeword 你好 --lang zh --cmd-config "$CFG" --channels 1 &
PID=$!
echo "demo pid=$PID"
# 让它运行若干秒（期间可对麦克风说话）
SLEEP=${1:-12}
echo "running $SLEEP seconds, please speak '你好, 打开灯' ..."
sleep "$SLEEP"
echo "=== sending SIGINT ==="
kill -INT "$PID"
wait "$PID"
echo "=== demo exited code $? ==="
