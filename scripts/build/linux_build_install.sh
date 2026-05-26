#!/bin/bash

sudo apt-get update
sudo apt-get install -y build-essential libncurses5-dev bison flex libssl-dev libelf-dev

cd platform/linux
# TODO: consider providing a config ourselves to make the build more deterministic
cp /boot/config-$(uname -r) .config
make olddefconfig

make -j$(nproc)

sudo make modules_install
# reboot to take effect
sudo make install
# install headers as well -- hemem links with headers from the custom linux build
sudo make headers_install
