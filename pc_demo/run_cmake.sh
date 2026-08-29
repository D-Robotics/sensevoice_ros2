#!/usr/bin/env bash
cd ~/pc_demo
rm -rf build
mkdir -p build
cd build
export SENSEVOICE_SRC_DIR=/home/shulu/SenseVoice.cpp
echo "SENSEVOICE_SRC_DIR env = $SENSEVOICE_SRC_DIR"
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  > /tmp/cmake_run.log 2>&1
echo "CMAKE_EXIT_CODE=$?" >> /tmp/cmake_run.log
cat /tmp/cmake_run.log
