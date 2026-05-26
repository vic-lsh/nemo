#!/bin/bash

QEMU_PATH=../platform/qemu/build/qemu-system-x86_64

sudo $QEMU_PATH \
    -enable-kvm \
    -m 2048 \
    -smp 2 \
    -name "vmtest" \
    -cpu host \
    -mem-path /dev/hugepages \
    -mem-prealloc \
    -drive file=vmtest.qcow2,if=virtio,format=qcow2 \
    -drive file=cidata.iso,if=virtio,format=raw \
    -net nic,model=virtio \
    -net user \
    -nographic \
    -incoming tcp:127.0.0.1:6666
