#!/usr/bin/env bash
cd ~/pc_demo/build
make -j"$(nproc)" > /tmp/make_run.log 2>&1
echo "MAKE_EXIT_CODE=$?" >> /tmp/make_run.log
tail -60 /tmp/make_run.log
