#!/usr/bin/env bash
export SENSEVOICE_SRC_DIR=/home/shulu/SenseVoice.cpp
cd ~/pc_demo
rm -rf build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release > /tmp/full_cmake.log 2>&1
echo "CMAKE_EXIT=$?"
make -j"$(nproc)" > /tmp/full_make.log 2>&1
echo "MAKE_EXIT=$?"
tail -45 /tmp/full_make.log
