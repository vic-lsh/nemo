#!/bin/sh

while :
do
    # numactl -N0 -m0 -- perf stat -e mem_load_l3_miss_retired.local_dram,mem_load_retired.local_pmm &
    numactl -N0 -m0 -- perf stat -e cache-misses &
    perf_pid=$!;
    sleep 1;
    kill -9 ${perf_pid};
done
