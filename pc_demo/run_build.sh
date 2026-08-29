#!/usr/bin/env bash
cd ~/pc_demo/build
cmake .. -DCMAKE_BUILD_TYPE=Release > /tmp/cmake2.log 2>&1
echo "CMAKE_EXIT=$?"
make -j"$(nproc)" > /tmp/make2.log 2>&1
echo "MAKE_EXIT=$?"
tail -40 /tmp/make2.log
