#!/bin/bash

# /dev/pmem0 should be created after reserving the cxl memory region during boot
[ -e /dev/pmem0 ] || { echo "/dev/pmem0 does not exist. Exiting." >&2; exit 1; }

sudo ndctl create-namespace -f -e namespace0.0 --mode=devdax --align 2M

[ -e /dev/dax0.0 ] || { echo "Error: did not successfully create cxl dax device." >&2; exit 1; }

echo "created cxl dax device at /dev/dax0.0"
