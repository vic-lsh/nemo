#!/bin/bash -x
sudo echo 1000000 > /proc/sys/vm/max_map_count

N_THREADS=6
N_SECS=300
WARMUP_SECS=20
HOT_FRAC=0.10
CHANGE_HOTSET_SECS=120
HOT_FRAC_AFTER=0.20
MEM_SZ=$((22*1024*1024*1024))
VAL_SZ=1024
GET_PROB=0.9
#ZIPF=0.1


shim_args=$@

./exp_scripts/nemo_shim.sh $shim_args \
    ./apps/flexkvs/kvsbench \
    -t $N_THREADS -T $N_SECS -w $WARMUP_SECS -h $HOT_FRAC \
    -D $CHANGE_HOTSET_SECS -H $HOT_FRAC_AFTER 127.0.0.1:11211 -S $MEM_SZ \
    -v $VAL_SZ -g $GET_PROB

