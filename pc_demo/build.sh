#!/usr/bin/env bash
# PC 版 SenseVoice demo 一键构建脚本（Ubuntu 22.04 x86_64）
# 首次运行会：
#   1. git clone 上游开源 lovemefan/SenseVoice.cpp（约几分钟）
#   2. cmake 配置 + 编译
# 需要联网。

set -euo pipefail
cd "$(dirname "$0")"

echo "==> 检查上游 SenseVoice.cpp 是否已克隆..."
if [ ! -d third_party/SenseVoice.cpp ]; then
  echo "==> 克隆 lovemefan/SenseVoice.cpp（含 ggml 子模块）..."
  git clone --depth 1 --recursive https://github.com/lovemefan/SenseVoice.cpp.git third_party/SenseVoice.cpp
fi

echo "==> 提示：请确认 ASR 模型文件已存在："
echo "    third_party/SenseVoice.cpp/models/sense-voice-small-fp16.gguf"
echo "    （缺失时可用 build_download_model.sh 从 ModelScope 下载）"

echo "==> CMake 配置 + 编译..."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc 2>/dev/null || echo 4)"

echo ""
echo "==> 构建完成：build/sensevoice_pc_demo"
echo "==> 运行示例："
echo "    ./build/sensevoice_pc_demo \\"
echo "      --model third_party/SenseVoice.cpp/models/sense-voice-small-fp16.gguf \\"
echo "      --device plughw:0,0 --wakeword 你好 --lang zh --cmd-config config/cmd_word.json"
