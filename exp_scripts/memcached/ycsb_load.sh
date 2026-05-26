#!/bin/bash

set -e

if [ $# -lt 3 ]; then
    echo "Error: No parameters provided."
    echo "Usage: $0 <workloada/b/c/d/e/f> <# of threads> <recordcount>"
    exit 1
fi

workload=$1
threads=$2
recordcount=$3
port=12345

cd apps/ycsb

sudo numactl -N0 -m0 --physcpubind=20-63\
    ./bin/ycsb load memcached -s -threads $threads \
                                -P workloads/$workload \
                                -p "memcached.hosts=localhost:$port" \
                                -p "recordcount=$recordcount" \

echo "Finished Memcached loading."
