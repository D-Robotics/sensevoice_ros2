#!/usr/bin/env bash
set +e
echo "=== Windows side model search ==="
ls -la /mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf 2>/dev/null || echo "NO MODEL at /mnt/d/work/github/SenseVoice.cpp/models"
ls -la /mnt/d/work/github/sensevoice_ros2/pc_demo/third_party/SenseVoice.cpp/models/sense-voice-small-fp16.gguf 2>/dev/null || echo "NO MODEL tp win"
echo "=== pulse server reachable? ==="
timeout 5 pactl info 2>&1 | head -5 || echo "pactl info failed"
echo "=== pulse sources (mic) ==="
pactl list short sources 2>/dev/null || echo "no sources"
echo "=== default source ==="
pactl get-default-source 2>/dev/null || echo "no default source"
