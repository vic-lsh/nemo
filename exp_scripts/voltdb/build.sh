#!/bin/bash

# install java on the host
sudo apt-get update
sudo apt-get install -y\
    wget\
    apt-transport-https\
    gpg
wget -qO - https://packages.adoptium.net/artifactory/api/gpg/key/public | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/adoptium.gpg > /dev/null
echo "deb https://packages.adoptium.net/artifactory/deb $(awk -F= '/^VERSION_CODENAME/{print$2}' /etc/os-release) main" | sudo tee /etc/apt/sources.list.d/adoptium.list
sudo apt-get update
sudo apt-get install -y\
    temurin-8-jdk

# install other build dependencies
sudo apt-get install -y\
    python3\
    git\
    ant\
    build-essential\
    ant-optional\
    valgrind\
    ntp\
    ccache\
    cmake

cd apps/voltdb

# build voltdb
ant
