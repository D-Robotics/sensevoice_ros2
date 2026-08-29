#!/usr/bin/env bash
# 从 ModelScope 下载 SenseVoice 模型到 third_party/SenseVoice.cpp/models/
# 参考：https://www.modelscope.cn/models/lovemefan/SenseVoiceGGUF
set -euo pipefail
cd "$(dirname "$0")"

MODEL_DIR="third_party/SenseVoice.cpp/models"
MODEL_URL="https://modelscope.cn/models/lovemefan/SenseVoiceGGUF/resolve/master/sense-voice-small-fp16.gguf"

mkdir -p "${MODEL_DIR}"
if [ ! -f "${MODEL_DIR}/sense-voice-small-fp16.gguf" ]; then
  echo "==> 下载 sense-voice-small-fp16.gguf ..."
  if command -v aria2c >/dev/null 2>&1; then
    aria2c -x 4 -s 4 -d "${MODEL_DIR}" -o sense-voice-small-fp16.gguf "${MODEL_URL}"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "${MODEL_DIR}/sense-voice-small-fp16.gguf" "${MODEL_URL}"
  else
    curl -L -o "${MODEL_DIR}/sense-voice-small-fp16.gguf" "${MODEL_URL}"
  fi
else
  echo "==> 模型已存在，跳过下载。"
fi
ls -lh "${MODEL_DIR}/sense-voice-small-fp16.gguf"
