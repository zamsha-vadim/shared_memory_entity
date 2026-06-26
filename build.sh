#!/bin/bash

set -e

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
make -j`nproc` -C build

# Demo
cmake -B build/demo -S demo -DSme_DIR=$PWD/build/share/cmake -DCMAKE_BUILD_TYPE=Release
make -j`nproc` -C build/demo

