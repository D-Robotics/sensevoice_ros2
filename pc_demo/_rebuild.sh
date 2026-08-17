#!/usr/bin/env bash
set -e
export SENSEVOICE_SRC_DIR=/home/shulu/SenseVoice.cpp
cd /home/shulu/pc_demo
echo "=== cmake ==="
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSENSEVOICE_SRC_DIR="$SENSEVOICE_SRC_DIR"
echo "=== make ==="
cmake --build build -j"$(nproc)"
echo "=== binary ==="
ls -la build/bin/sensevoice_pc_demo
echo "BUILD_OK"
