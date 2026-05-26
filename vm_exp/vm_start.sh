#!/bin/bash

set -x

set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <disk-directory>"
    exit 1
fi

DISK_DIR="$1"
VM_DISK_PATH="${DISK_DIR}/my-cloud-vm.qcow2"
CIDATA_ISO_PATH="${DISK_DIR}/cidata.iso"
CONF_FILE_PATH="${DISK_DIR}/vm.conf"

if [ ! -f "$VM_DISK_PATH" ]; then
    echo "Error: VM disk image '$VM_DISK_PATH' not found."
    exit 1
fi

if [ ! -f "$CIDATA_ISO_PATH" ]; then
    echo "Error: cloud-init ISO '$CIDATA_ISO_PATH' not found."
    exit 1
fi

INITRD_IMG_PATH=""
VMLINUZ_PATH=""
KERNEL_CMDLINE=""

if [ -f "$CONF_FILE_PATH" ]; then
    # shellcheck disable=SC1090
    source "$CONF_FILE_PATH"
fi

if [ -n "$INITRD_IMG_PATH" ] && [[ "$INITRD_IMG_PATH" != /* ]]; then
    INITRD_IMG_PATH="${DISK_DIR}/${INITRD_IMG_PATH}"
fi

if [ -n "$VMLINUZ_PATH" ] && [[ "$VMLINUZ_PATH" != /* ]]; then
    VMLINUZ_PATH="${DISK_DIR}/${VMLINUZ_PATH}"
fi

if [ -n "$INITRD_IMG_PATH" ] && [ ! -f "$INITRD_IMG_PATH" ]; then
    echo "Error: initrd image '$INITRD_IMG_PATH' not found."
    exit 1
fi

if [ -n "$VMLINUZ_PATH" ] && [ ! -f "$VMLINUZ_PATH" ]; then
    echo "Error: vmlinuz '$VMLINUZ_PATH' not found."
    exit 1
fi

LIBHEMEM_PATH=../build/libhemem-qemu.so

QEMU_ARGS=(
    -enable-kvm
    -m 4G
    -smp 2
    -name "vmtest"
    -cpu host
    -drive "file=$VM_DISK_PATH,if=virtio,format=qcow2"
    -drive "file=$CIDATA_ISO_PATH,if=virtio,format=raw"
    -net "nic,model=virtio"
    -net "user,hostfwd=tcp::2222-:22"
    -nographic
    -monitor "telnet::4444,server,nowait"
    -device vfio-pci,host=40:00.0
    -device vfio-pci,host=40:00.1
    # -M q35,nvdimm=on
    # -m 8G,slots=4,maxmem=32G
    # -object memory-backend-file,id=mem1,mem-path=/dev/dax0.0,size=1G,share=on,align=2M
    # -device nvdimm,memdev=mem1,id=nvdimm1
)

if [ -n "$VMLINUZ_PATH" ]; then
    QEMU_ARGS+=(-kernel "$VMLINUZ_PATH")
fi

if [ -n "$INITRD_IMG_PATH" ]; then
    QEMU_ARGS+=(-initrd "$INITRD_IMG_PATH")
fi

if [ -n "$KERNEL_CMDLINE" ]; then
    QEMU_ARGS+=(-append "$KERNEL_CMDLINE")
fi

qemu-system-x86_64 "${QEMU_ARGS[@]}"
