#!/usr/bin/env bash
set +e
WIN_PC=/mnt/d/work/github/sensevoice_ros2/pc_demo
WSL_PC=/home/shulu/pc_demo

echo "=== sync src ==="
cp -v "$WIN_PC/src/main.cpp" "$WSL_PC/src/main.cpp"
cp -v "$WIN_PC/src/sense_engine.cpp" "$WSL_PC/src/sense_engine.cpp"
cp -v "$WIN_PC/README.md" "$WSL_PC/README.md"

echo "=== rebuild ==="
export SENSEVOICE_SRC_DIR=/home/shulu/SenseVoice.cpp
cd "$WSL_PC"
cmake --build build -j"$(nproc)"
echo "BUILD_RC=$?"
ls -la "$WSL_PC/build/bin/sensevoice_pc_demo"
echo "[done]"
