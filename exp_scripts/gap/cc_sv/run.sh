#!/bin/bash -x

if [ ! -d "exp_scripts" ]; then
    echo "Error: exp_scripts must be run from project root."
    exit 1
fi

source ./exp_scripts/config.sh
source ./exp_scripts/gap/cc_sv/config.sh

no_intercept=false
for arg in "$@"
do
    if [ "$arg" == "--no-intercept" ]; then
        no_intercept=true
        break
    fi
done

ld_preload=
if $no_intercept; then
    echo "Running without hemem."
else
    echo "Running with hemem."
    ld_preload="LD_PRELOAD=$HEMEM_LIB"
fi

end_cpu=$((HEMEM_APP_CPU_START + n_cpus - 1))

sudo nice -20 numactl -N0 -m0 --physcpubind=$HEMEM_APP_CPU_START-$end_cpu -- \
    env MISS_RATIO=0.0 \
    $ld_preload ./apps/gapbs/cc_sv \
        -g $graph_size_log \
        -k $avg_degree \
        -n $n_trials \
        | tee gap_cc_sv.txt
