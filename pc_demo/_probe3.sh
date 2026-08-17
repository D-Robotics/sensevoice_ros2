#!/usr/bin/env bash
set +e
echo "=== pulse modules (look for rdp / module-native-protocol) ==="
pactl list short modules 2>/dev/null | head -40
echo "=== sinks ==="
pactl list short sinks 2>/dev/null
echo "=== all sources with state ==="
pactl list sources 2>/dev/null | grep -E "Name:|State:" 
echo "=== windows mic? check via pactl card ==="
pactl list short cards 2>/dev/null
echo "=== test capture 1s from default source ==="
timeout 2 parec -d RDPSource --raw 2>/dev/null | wc -c || echo "parec failed"
