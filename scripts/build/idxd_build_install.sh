#!/bin/bash

sudo apt-get install -y build-essential
sudo apt-get install -y autoconf automake autotools-dev libtool pkgconf asciidoc xmlto
sudo apt-get install -y uuid-dev libjson-c-dev libkeyutils-dev libz-dev libssl-dev
sudo apt-get install -y debhelper devscripts debmake quilt fakeroot lintian asciidoctor
sudo apt-get install -y file gnupg patch patchutils

cd ./3rd_party/idxd-config

./autogen.sh
./configure CFLAGS='-g -O2' --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib64
make
make check
sudo make install
