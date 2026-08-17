#!/usr/bin/env bash
cd ~/pc_demo
rm -rf build
mkdir -p build
cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DSENSEVOICE_SRC_DIR="$HOME/SenseVoice.cpp" \
  > /tmp/cmake.log 2>&1
echo "=== CMAKE EXIT: $? ==="
tail -25 /tmp/cmake.log
echo "=== BUILD ==="
make -j"$(nproc)" > /tmp/make.log 2>&1
echo "=== MAKE EXIT: $? ==="
tail -50 /tmp/make.log
echo "=== DONE ==="
ls -la sensevoice_pc_demo 2>/dev/null && echo "BINARY_OK"
