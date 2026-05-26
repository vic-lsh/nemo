#!/bin/bash -x

sudo apt-get update
sudo apt-get install -y\
    maven

cd apps/hibench

mvn -Psparkbench -Dspark=2.4 -Dscala=2.11 clean package
