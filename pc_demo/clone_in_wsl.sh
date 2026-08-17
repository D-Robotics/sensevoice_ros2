#!/usr/bin/env bash
# 在 WSL2 内重试克隆上游 SenseVoice.cpp（含 ggml 子模块）
set -u
cd ~ || exit 1
rm -rf ~/SenseVoice.cpp

for i in 1 2 3 4 5; do
  echo "=== clone attempt $i ==="
  if git clone --recurse-submodules --depth 1 \
      https://github.com/lovemefan/SenseVoice.cpp.git ~/SenseVoice.cpp; then
    echo "CLONE_OK"
    break
  fi
  echo "retry after 5s..."
  rm -rf ~/SenseVoice.cpp
  sleep 5
done

if [ -d ~/SenseVoice.cpp/.git ]; then
  echo "=== verify submodules ==="
  cd ~/SenseVoice.cpp
  ls sense-voice/csrc/third-party/ggml/ 2>/dev/null | head -5 || echo "GGML STILL MISSING"
  echo "=== DONE ==="
else
  echo "CLONE_FAILED"
  exit 1
fi
