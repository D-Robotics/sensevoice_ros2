#!/usr/bin/env bash
set +e
echo "=== /etc/asound.conf ==="
cat /etc/asound.conf 2>/dev/null
echo "[end asound.conf]"
echo "=== /home/shulu/.asoundrc ==="
cat /home/shulu/.asoundrc 2>/dev/null
echo "[end asoundrc]"
echo "=== pulse alsa plugin ==="
ls /usr/lib/x86_64-linux-gnu/alsa-lib/ 2>/dev/null | grep -i pulse
echo "[end]"
