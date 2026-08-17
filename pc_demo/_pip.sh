#!/usr/bin/env bash
set +e
echo "=== python3 -m pip ? ==="
python3 -m pip --version 2>&1 | head -1 || echo "no pip module"
echo "=== ensurepip ==="
python3 -m ensurepip 2>&1 | tail -2 || echo "ensurepip failed"
echo "=== try install miniaudio via python3 -m pip ==="
python3 -m pip install --quiet miniaudio 2>&1 | tail -3
python3 -c "import miniaudio; print('miniaudio OK')" 2>&1 | tail -1
echo "[done]"
