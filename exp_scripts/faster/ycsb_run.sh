#!/bin/bash -x

if [ $# -lt 1 ]; then
    echo "Usage: ./exp_scripts/faster/ycsb_run.sh <workload>"
    echo "see pmem_benchmark.cc in apps/faster to see available workloads."
    exit 1
fi

if [[ -v NEMO_CI ]]; then
    echo "NEMO_CI is set. Using CI config..."
    source ./exp_scripts/faster/config_ci.sh
else
    source ./exp_scripts/faster/config.sh
fi

faster_dir=apps/faster/cc/build/Release

workload=$0
shift 1
shim_args=$@

./exp_scripts/nemo_shim.sh $shim_args \
    $faster_dir/pmem_benchmark $workload \
    $faster_n_load_thrs $faster_n_run_thrs $faster_zipf $faster_n_records \
    $faster_n_ops $faster_n_warmup_ops $faster_max_runtime_secs
