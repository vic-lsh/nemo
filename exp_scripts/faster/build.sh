#!/bin/bash -x

sudo apt-get update
sudo apt-get install -y \
    libaio-dev \
    libnuma-dev \
    libtbb-dev

cd apps/faster/cc
mkdir -p build/Release
cd build/Release
cmake -DCMAKE_BUILD_TYPE=Release ../..
make pmem_benchmark
