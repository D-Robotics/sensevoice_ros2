#!/usr/bin/env bash
set +e
cd /home/shulu/pc_demo
echo "=== try install miniaudio (pure-wheel mp3 decoder) ==="
pip3 install --quiet miniaudio 2>&1 | tail -3
python3 - <<'PY'
import miniaudio, wave, struct
src = "test_src.bin"   # the MP3 we downloaded
print("decoding", src)
info = miniaudio.mp3_read_file_f32(src)
print("channels", info.channels, "sample_rate", info.sample_rate, "frames", info.frames)
# downmix to mono + resample to 16000
import math
# miniaudio returns interleaved f32 for info.channels
ch = info.channels
data = info.samples  # list of float, length = frames*ch
n = info.frames
mono = [sum(data[i*ch + c] for c in range(ch))/ch for i in range(n)]
# simple resample to 16000
ratio = 16000.0 / info.sample_rate
out_n = int(n * ratio)
out = []
for i in range(out_n):
    pos = i / ratio
    i0 = min(int(pos), n-1); i1 = min(i0+1, n-1)
    f = pos - i0
    out.append(mono[i0]*(1-f) + mono[i1]*f)
# write 16-bit wav
w = wave.open("test_zh.wav", "wb")
w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000)
import array
a = array.array("h")
for v in out:
    v = max(-1.0, min(1.0, v))
    a.append(int(v*32767))
w.writeframes(a.tobytes())
w.close()
print("wrote test_zh.wav, samples", len(out), "dur", len(out)/16000.0)
PY
ls -la test_zh.wav 2>/dev/null || echo "NO WAV"
echo "[done]"
