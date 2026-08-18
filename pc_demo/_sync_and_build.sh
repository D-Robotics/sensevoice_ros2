#!/usr/bin/env bash
set -e
SRC=/mnt/d/work/github/sensevoice_ros2/pc_demo
DST=/home/shulu/pc_demo
echo "=== 同步源码与配置到 /home/shulu/pc_demo ==="
cp -v "$SRC/src/tts_engine.cpp"      "$DST/src/tts_engine.cpp"
cp -v "$SRC/include/tts_engine.h"     "$DST/include/tts_engine.h"
cp -v "$SRC/config/cmd_word.json"    "$DST/config/cmd_word.json"
cp -v "$SRC/CMakeLists.txt"          "$DST/CMakeLists.txt" 2>/dev/null || true
cp -v "$SRC/test_live.sh"            "$DST/test_live.sh" 2>/dev/null || true
echo "=== 重新构建 ==="
cd "$DST/build"
cmake --build . -j"$(nproc)" 2>&1 | tail -20
echo "=== build exit: ${PIPESTATUS[0]} ==="
ls -l "$DST/build/bin/sensevoice_pc_demo"
