#!/usr/bin/env bash
set +e
WIN_PC=/mnt/d/work/github/sensevoice_ros2/pc_demo
WSL_PC=/home/shulu/pc_demo
echo "=== sync ALL sources ==="
cp -v "$WIN_PC/src/main.cpp"         "$WSL_PC/src/main.cpp"
cp -v "$WIN_PC/src/sense_engine.cpp" "$WSL_PC/src/sense_engine.cpp"
cp -v "$WIN_PC/src/alsa_audio.cpp"   "$WSL_PC/src/alsa_audio.cpp"
cp -v "$WIN_PC/include/sense_engine.h" "$WSL_PC/include/sense_engine.h"
cp -v "$WIN_PC/include/alsa_audio.h"  "$WSL_PC/include/alsa_audio.h"
cp -v "$WIN_PC/CMakeLists.txt"       "$WSL_PC/CMakeLists.txt"
export SENSEVOICE_SRC_DIR=/home/shulu/SenseVoice.cpp
cd "$WSL_PC"
cmake --build build -j"$(nproc)" 2>&1 | tail -5
echo "=== run file mode ==="
stdbuf -oL -eL "$WSL_PC/build/bin/sensevoice_pc_demo" \
  --model /mnt/d/work/github/SenseVoice.cpp/models/sense-voice-small-fp16.gguf \
  --file /mnt/d/work/github/sensevoice_ros2/pc_demo/test_zh.wav \
  --wakeword 你好 --lang zh \
  --cmd-config "$WSL_PC/config/cmd_word.json" --channels 1 2>&1 | grep -E "\[dbg\]|\[ASR\]|\[CMD\]|file mode|init ok|bye|consumed"
echo "=== end ==="
