#!/usr/bin/env bash
cd ~/pc_demo/build
make -j"$(nproc)" > /tmp/make3.log 2>&1
echo "MAKE_EXIT=$?"
tail -20 /tmp/make3.log
