#!/bin/bash

if [ ! -d "exp_scripts" ]; then
    # sanity check that we're at the project root
    echo "Error: exp_scripts must be run from project root."
    exit 1
fi


sudo apt-get install -y\
    cmake

# get all our dependencies
git submodule update --init --recursive

./scripts/build/idxd_build_install.sh
./scripts/build/syscall_intercept_build_install.sh

# install ruby, required by the project test runner
./scripts/build/ruby_build_install.sh

# build and install linux kernel with hemem patches
./scripts/build/linux_build_install.sh

cd build
cmake ..
