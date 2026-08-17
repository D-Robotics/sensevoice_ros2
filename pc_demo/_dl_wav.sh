#!/usr/bin/env bash
set +e
cd /home/shulu/pc_demo
echo "=== try download SenseVoice official test wav (zh) ==="
# FunAudioLLM / SenseVoice 官方示例音频
URLS=(
  "https://modelscope.cn/models/iic/SenseVoiceSmall/resolve/master/example/zh.mp3"
  "https://github.com/FunAudioLLM/SenseVoice/raw/main/example/zh.wav"
)
for u in "${URLS[@]}"; do
  echo "trying: $u"
  if timeout 30 wget -q -O test_audio.bin "$u"; then
    echo "OK downloaded $(stat -c%s test_audio.bin) bytes"
    file test_audio.bin 2>/dev/null
    mv -f test_audio.bin test_src.bin
    break
  else
    echo "failed: $u"
  fi
done
ls -la test_src.bin 2>/dev/null || echo "NO TEST AUDIO"
echo "[done]"
