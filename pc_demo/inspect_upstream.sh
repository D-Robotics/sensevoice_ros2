#!/usr/bin/env bash
set -e
cd ~/SenseVoice.cpp
echo "=== top CMakeLists.txt (first 70 lines) ==="
sed -n '1,70p' CMakeLists.txt
echo "=== ls sense-voice/ ==="
ls sense-voice/
echo "=== find sense-voice.h ==="
find . -name "sense-voice.h" 2>/dev/null
echo "=== grep add_library in sense-voice/csrc ==="
grep -rn "add_library\|add_subdirectory" sense-voice/csrc/CMakeLists.txt 2>/dev/null | head -20
echo "=== find .so / .a built? ==="
find . -name "*.so" -o -name "*.a" 2>/dev/null | grep -i sense | head
