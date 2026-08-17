#!/usr/bin/env bash
set +e
WSL_PC=/home/shulu/pc_demo
BIN="$WSL_PC/build/bin/sensevoice_pc_demo"
stdbuf -oL -eL "$BIN" \
  --model /mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf \
  --file /mnt/d/work/github/sensevoice_ros2/pc_demo/test_zh.wav \
  --wakeword 你好 --lang zh \
  --cmd-config "$WSL_PC/config/cmd_word.json" --channels 1 > /tmp/out.log 2>&1
echo "=== tail of output ==="
tail -30 /tmp/out.log
echo "=== grep dbg ==="
grep "dbg" /tmp/out.log || echo "NO DBG LINES"
echo "=== grep ASR/CMD ==="
grep -E "ASR|CMD" /tmp/out.log || echo "NO ASR/CMD"
