#!/bin/bash

sudo apt-get update
sudo apt-get install -y\
    clang\
    pkg-config\
    libcapstone-dev

cd ./3rd_party/syscall_intercept

mkdir -p build
cd build

cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang
make -j$(nproc)

sudo make install
