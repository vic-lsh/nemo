#!/bin/bash

N_GBS=$1
Kth_access_non_hot=$2
S_secs_hot_shift=$3
N_thrs=$4

HEMEM_PATH=./src/libhemem.so

taskset -c 12 numactl -N0 -m0 -- env LD_PRELOAD=$HEMEM_PATH ./microbenchmarks/mem-access \
    $N_GBS $Kth_access_non_hot $S_secs_hot_shift $N_thrs
