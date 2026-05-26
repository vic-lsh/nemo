#!/bin/bash -x

if [ ! -d "exp_scripts" ]; then
    echo "Error: exp_scripts must be run from project root."
    exit 1
fi

source ./exp_scripts/config.sh
source ./exp_scripts/gap/bc/config.sh

# Run with PIN
# sudo nice -20 numactl -N0 -m0 --physcpubind=3-5 -- env MISS_RATIO=0.0 \
#     ../pin-331/pin -t ../pin-331/source/tools/MyPinTool/obj-intel64/MyPinTool.so -- \
#     ./apps/gapbs/bc -g $1

shim_args=$@

sudo ./exp_scripts/nemo_shim.sh $shim_args \
    ./apps/gapbs/bc \
        -g $graph_size_log \
        -k $avg_degree \
        -n $n_trials \
        -i $n_iterations\
        | tee gap_bc.txt
