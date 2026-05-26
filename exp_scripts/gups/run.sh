#!/bin/bash -x
export LD_LIBRARY_PATH=./src:./Hoard/src:$LD_LIBRARY_PATH
echo 1000000 > /proc/sys/vm/max_map_count

#HEMEM_PATH=/mnt/sda1/projs/cxl-tm-host/src/libhemem.so
HEMEM_PATH=/mnt/sda1/projs/cxl-tm-host/build/libhemem.so

debugfile=/tmp/debug.txt

gups_pid=
central_pid=
run_perf_pid=

THREADS=8
UPDATES_PER_THR=0 # 0 = unlimited
LOG_DATA_SZ=35 # log size of the region
DATA_SZ_BYTES=8
LOG_HOT_SZ=33 # log size of hot set
WAIT=0 # 0 = no-wait
FNAME=data/dynamic/perf/gups.txt

nice -20 numactl -N0 -m0 --physcpubind=17-36 -- env MISS_RATIO=0.0 \
    LD_PRELOAD=$HEMEM_PATH ./microbenchmarks/gups-pebs \
    $THREADS $UPDATES_PER_THR $LOG_DATA_SZ $DATA_SZ_BYTES $LOG_HOT_SZ $WAIT $FNAME

# ctrl_c_handler() {
#     echo ""
#     echo "Caught Ctrl-C, exiting..."
#     cleanup
#     exit 1
# }
#
# cleanup() {
#     kill -s USR2 $gups_pid
#     kill -9 ${central_pid}
#     kill -9 ${run_perf_pid}
#     pkill perf
# }
#
# trap ctrl_c_handler SIGINT
#
# ./run_perf.sh >/dev/null 2>&1 &
# run_perf_pid=$!
#
# nice -20 numactl -N0 -m0 --physcpubind=0-3 -- ./src/central-manager >$debugfile 2>&1 &
# central_pid=$!
# sleep 30
#
# rm data/dynamic/perf/gups.txt
# sleep 1
# nice -20 numactl -N0 -m0 --physcpubind=4-13 -- env MISS_RATIO=0.1 LD_PRELOAD=$HEMEM_PATH ./microbenchmarks/gups-pebs 4 0 38 8 36 0 data/dynamic/perf/gups.txt &
# gups_pid=$!
# sleep 240
# kill -s USR2 $gups_pid
# sleep 1
# kill -9 ${central_pid}
# kill -9 ${run_perf_pid}
# pkill perf
#
# cp /tmp/log-$gups_pid.txt data/dynamic/logs/gups-log.txt
