#!/bin/bash

# Define the target directory for hugetlbfs
HUGEPAGE_MOUNT_POINT="/mnt/nemo_shm"
HUGEPAGE_FSTYPE="hugetlbfs"

echo "Checking if '$HUGEPAGE_MOUNT_POINT' is mounted as $HUGEPAGE_FSTYPE..."

# Check if the directory is mounted as hugetlbfs using findmnt
# We redirect stdout to /dev/null and check the exit code.
if findmnt -t "${HUGEPAGE_FSTYPE}" "${HUGEPAGE_MOUNT_POINT}" >/dev/null 2>&1; then
    echo "'$HUGEPAGE_MOUNT_POINT' is already mounted as $HUGEPAGE_FSTYPE."
else
    echo "'$HUGEPAGE_MOUNT_POINT' is not mounted as $HUGEPAGE_FSTYPE. Attempting to mount..."

    # Create the directory if it doesn't exist
    if [ ! -d "$HUGEPAGE_MOUNT_POINT" ]; then
        echo "Creating directory '$HUGEPAGE_MOUNT_POINT'..."
        sudo mkdir -p "$HUGEPAGE_MOUNT_POINT"
        if [ $? -ne 0 ]; then
            echo "Error: Failed to create directory '$HUGEPAGE_MOUNT_POINT'. Aborting."
            exit 1
        fi
    fi

    # Attempt to mount hugetlbfs
    echo "Mounting $HUGEPAGE_FSTYPE to '$HUGEPAGE_MOUNT_POINT'..."
    sudo mount -t "${HUGEPAGE_FSTYPE}" none "$HUGEPAGE_MOUNT_POINT"

    # Check if the mount command was successful
    if [ $? -eq 0 ]; then
        echo "Successfully mounted $HUGEPAGE_FSTYPE to '$HUGEPAGE_MOUNT_POINT'."
        # Verify the mount again
        if findmnt -t "${HUGEPAGE_FSTYPE}" "${HUGEPAGE_MOUNT_POINT}" >/dev/null 2>&1; then
            echo "Verification successful: '$HUGEPAGE_MOUNT_POINT' is now mounted as $HUGEPAGE_FSTYPE."
        else
            echo "Warning: Mount command succeeded, but verification failed. Check mount status manually."
        fi
    else
        echo "Error: Failed to mount $HUGEPAGE_FSTYPE to '$HUGEPAGE_MOUNT_POINT'."
        echo "Please check permissions, kernel hugetlbfs configuration (e.g., vm.nr_hugepages), or if another filesystem is already on top."
        exit 1
    fi
fi

exit 0
