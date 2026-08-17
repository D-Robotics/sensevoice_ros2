#!/usr/bin/env bash
set +e
echo "=== ffmpeg ==="; which ffmpeg || echo "no ffmpeg"
echo "=== sox ==="; which sox || echo "no sox"
echo "=== espeak ==="; which espeak espeak-ng 2>/dev/null || echo "no espeak"
echo "=== python ==="; which python3 || echo "no python3"
echo "=== any wav in upstream models/test ==="
find /home/shulu/SenseVoice.cpp -iname "*.wav" 2>/dev/null | head
echo "=== check upstream example for wav reading ==="
ls /home/shulu/SenseVoice.cpp/sense-voice/csrc/examples/ 2>/dev/null || echo "no examples dir"
echo "[done]"
