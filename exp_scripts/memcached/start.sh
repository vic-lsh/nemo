#!/bin/bash -x

set -e

source ./exp_scripts/config.sh

CPUS=3-5
MEM_MB=1024
PORT=12345

sudo env nice -20 numactl -N0 -m0 --physcpubind=$CPUS -- \
    env LD_PRELOAD=$HEMEM_LIB \
    memcached -m $MEM_MB -p $PORT -u root
