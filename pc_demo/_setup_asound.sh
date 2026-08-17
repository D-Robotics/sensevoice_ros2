#!/usr/bin/env bash
set +e
# 让 ALSA 的 default 设备走 PulseAudio（WSLg 桥接 Windows 音频）
A="$HOME/.asoundrc"
cat > "$A" <<'EOF'
pcm.!default {
    type pulse
    # 不指定 sink/source，使用 PulseAudio 当前默认设备
}
ctl.!default {
    type pulse
}
EOF
echo "wrote $A:"
cat "$A"
echo "=== verify default source ==="
pactl get-default-source
echo "=== test capture 2s from default (alsa pulse) ==="
timeout 2 arecord -D default -f S16_LE -r 16000 -c 1 -t raw 2>/dev/null | wc -c
echo "[capture test done]"
