#!/bin/bash

set -e

if [ $# -lt 4 ]; then
    echo "Error: No parameters provided."
    echo "Usage: $0 <workloada/b/c/d/e/f> <# of threads> <recordcount> <operationcount>"
    exit 1
fi

workload=$1
threads=$2
recordcount=$3
operationcount=$4
port=12345

cd apps/ycsb

sudo numactl -N0 -m0 --physcpubind=34-40\
    timeout 120s \
    ./bin/ycsb run memcached -s -threads $threads \
                                -P workloads/$workload \
                                -p "memcached.hosts=localhost:$port" \
                                -p status.interval=1 \
                                -p "recordcount=$recordcount" \
                                -p "operationcount=$operationcount" \
                                2>&1

echo "Finished Memcached benchmark."
