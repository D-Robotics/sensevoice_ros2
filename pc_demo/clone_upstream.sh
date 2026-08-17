#!/usr/bin/env bash
# 在 WSL 内重试克隆上游 SenseVoice.cpp（网络不稳定时使用）
set -u
TARGET="$HOME/pc_demo/third_party/SenseVoice.cpp"
mkdir -p "$HOME/pc_demo/third_party"

for i in 1 2 3 4 5; do
  echo "=== clone attempt $i ==="
  if git clone --depth 1 https://github.com/lovemefan/SenseVoice.cpp.git "$TARGET" 2>&1 | tail -8; then
    echo "CLONE_OK"
    break
  fi
  echo "retry after 3s..."
  rm -rf "$TARGET"
  sleep 3
done

if [ -d "$TARGET/.git" ]; then
  echo "=== init submodules ==="
  cd "$TARGET"
  git submodule update --init --recursive --depth 1 2>&1 | tail -8 || echo "submodule failed (will try during cmake)"
  echo "DONE"
else
  echo "CLONE_FAILED"
fi
