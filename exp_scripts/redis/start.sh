#!/bin/bash -x

set -e

PORT=12345

./exp_scripts/nemo_shim.sh $@ redis-server --port $PORT
