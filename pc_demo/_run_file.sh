#!/usr/bin/env bash
set +e
MODEL=/mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf
BIN=/home/shulu/pc_demo/build/bin/sensevoice_pc_demo
CFG=/home/shulu/pc_demo/config/cmd_word.json
WAV=/mnt/d/work/github/sensevoice_ros2/pc_demo/test_zh.wav

if [ ! -f "$WAV" ]; then echo "NO WAV: $WAV"; exit 1; fi
if [ ! -x "$BIN" ]; then echo "NO BIN"; exit 1; fi

echo "=== run file mode ==="
"$BIN" --model "$MODEL" --file "$WAV" --wakeword 你好 --lang zh --cmd-config "$CFG" --channels 1
echo "=== exit $? ==="
