#!/bin/bash

set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <output-directory>"
    exit 1
fi

OUTPUT_DIR="$1"
mkdir -p "$OUTPUT_DIR"

VM_NAME="my-cloud-vm"
CLOUD_IMAGE_PATH="${OUTPUT_DIR}/testimg.img"
VM_DISK_PATH="${OUTPUT_DIR}/${VM_NAME}.qcow2"
DISK_SIZE="20G"

wget -O "$CLOUD_IMAGE_PATH" https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-amd64.img

echo "copying cloud image to disk image path '$VM_DISK_PATH'"
cp "${CLOUD_IMAGE_PATH}" "${VM_DISK_PATH}"

echo "resizing '$VM_DISK_PATH' to size $DISK_SIZE"
qemu-img resize "${VM_DISK_PATH}" "${DISK_SIZE}"

echo "done!"
