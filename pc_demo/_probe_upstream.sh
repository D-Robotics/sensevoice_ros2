#!/usr/bin/env bash
set +e
echo "=== upstream tree (top) ==="
ls -la /home/shulu/SenseVoice.cpp/
echo "=== look for wav/audio assets anywhere in upstream ==="
find /home/shulu/SenseVoice.cpp -maxdepth 3 -iname "*.wav" -o -iname "*.mp3" 2>/dev/null | head
echo "=== models dir ==="
ls -la /home/shulu/SenseVoice.cpp/models/ 2>/dev/null
echo "=== grep for wav reading in upstream csrc ==="
grep -rl "wav\|sndfile\|ReadWav\|load_wav" /home/shulu/SenseVoice.cpp/sense-voice/csrc --include=*.cpp --include=*.h 2>/dev/null | head
echo "[done]"
