#!/bin/bash -x

source ./exp_scripts/silo/config.sh

shim_args=$@

./exp_scripts/nemo_shim.sh $shim_args \
    ./apps/silo/silo/out-perf.masstree/benchmarks/dbtest --verbose \
    --bench $bench \
    --num-threads $num_threads \
    --scale-factor $scale_factor \
    --runtime $runtime \
    --numa-memory $numa_memory
