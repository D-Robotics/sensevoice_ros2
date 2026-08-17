#!/usr/bin/env bash
set +e
WSL_PC=/home/shulu/pc_demo
BIN="$WSL_PC/build/bin/sensevoice_pc_demo"
"$BIN" \
  --model /mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf \
  --file /mnt/d/work/github/sensevoice_ros2/pc_demo/test_zh.wav \
  --wakeword 你好 --lang zh \
  --cmd-config "$WSL_PC/config/cmd_word.json" --channels 1 2>&1 \
  | grep -E "\[dbg\]|\[ASR\]|\[CMD\]|file mode|bye|init ok"
echo "=== exit ==="
