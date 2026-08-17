#!/usr/bin/env bash
set +e
echo "=== ~/.asoundrc ==="
cat /home/shulu/.asoundrc 2>/dev/null || echo "MISSING"
echo "=== cmd_word.json ==="
ls -la /home/shulu/pc_demo/config/cmd_word.json 2>/dev/null || echo "MISSING cmd_word.json"
echo "=== test_live.sh ==="
ls -la /home/shulu/pc_demo/test_live.sh 2>/dev/null || echo "MISSING test_live.sh"
echo "=== current default source ==="
pactl get-default-source
echo "[done]"
