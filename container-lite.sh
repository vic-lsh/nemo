#!/bin/bash
# container-with-preload.sh

usage() {
    echo "Usage: $0 [--preload=/path/to/lib.so] <program> [args...]"
    echo "  --preload: Path to library to LD_PRELOAD (can be used multiple times)"
    echo "Example: $0 --preload=/usr/lib/libtest.so --preload=/home/user/mylib.so myprogram arg1 arg2"
    exit 1
}

PRELOAD_LIBS=()
PROGRAM=""
ARGS=()

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --preload=*)
            PRELOAD_LIBS+=("${1#*=}")
            shift
            ;;
        --preload)
            if [[ -n "$2" && "$2" != --* ]]; then
                PRELOAD_LIBS+=("$2")
                shift 2
            else
                echo "Error: --preload requires a library path"
                usage
            fi
            ;;
        -h|--help)
            usage
            ;;
        *)
            if [[ -z "$PROGRAM" ]]; then
                PROGRAM="$1"
            else
                ARGS+=("$1")
            fi
            shift
            ;;
    esac
done

if [[ -z "$PROGRAM" ]]; then
    echo "Error: No program specified"
    usage
fi

CONTAINER_ID="simple-$(date +%s)"
ROOTFS="/tmp/rootfs-$CONTAINER_ID"

# Create minimal environment
sudo mkdir -p "$ROOTFS"/{bin,lib,lib64,proc,sys,dev,tmp}

copy_with_deps() {
    local file_path="$1"
    local dest_root="$2"
    
    if [[ ! -f "$file_path" ]]; then
        echo "Warning: File $file_path not found"
        return 1
    fi
    
    echo "Copying $file_path to rootfs..."
    
    # For executables, always copy to /bin regardless of original location
    if [[ -x "$file_path" ]] && file "$file_path" | grep -q "executable"; then
        sudo cp "$file_path" "$dest_root/bin/"
        echo "  → Copied executable to /bin/$(basename "$file_path")"
    else
        # For libraries, preserve the directory structure
        local dest_dir="$dest_root$(dirname "$file_path")"
        sudo mkdir -p "$dest_dir"
        sudo cp "$file_path" "$dest_dir/"
        echo "  → Copied library to $dest_dir/$(basename "$file_path")"
    fi
    
    # Copy dependencies if it's an executable/library
    if ldd "$file_path" >/dev/null 2>&1; then
        echo "  → Copying dependencies..."
        ldd "$file_path" | grep -o '/[^ ]*' | while read lib; do
            if [[ -f "$lib" ]]; then
                local lib_dest_dir="$dest_root$(dirname "$lib")"
                sudo mkdir -p "$lib_dest_dir"
                if sudo cp "$lib" "$lib_dest_dir/" 2>/dev/null; then
                    echo "    → $lib"
                fi
            fi
        done
    fi
}

# Copy program and dependencies
PROG_PATH=""
if command -v "$PROGRAM" >/dev/null; then
    PROG_PATH=$(command -v "$PROGRAM")
    echo "Copying program from PATH: $PROG_PATH"
    copy_with_deps "$PROG_PATH" "$ROOTFS"
elif [[ -f "$PROGRAM" ]]; then
    PROG_PATH="$PROGRAM"
    echo "Copying program from direct path: $PROG_PATH"
    # For direct paths, copy to /bin/ in the rootfs
    sudo cp "$PROG_PATH" "$ROOTFS/bin/$(basename "$PROGRAM")"
    copy_with_deps "$PROG_PATH" "$ROOTFS"
else
    echo "Error: Program '$PROGRAM' not found in PATH or as file"
    exit 1
fi

PROG_BASENAME=$(basename "$PROG_PATH")

# Copy LD_PRELOAD libraries and their dependencies
PRELOAD_PATHS=()
for lib in "${PRELOAD_LIBS[@]}"; do
    echo "Copying preload library: $lib"
    if copy_with_deps "$lib" "$ROOTFS/"; then
        PRELOAD_PATHS+=("$lib")
        echo "  → Library will be available at: $lib (inside chroot)"
    else
        echo "Error: Failed to copy preload library $lib"
        exit 1
    fi
done

# Copy essential utilities (env is needed for LD_PRELOAD)
echo "Copying essential utilities..."
for util in /usr/bin/env /bin/env; do
    if [[ -f "$util" ]]; then
        echo "Copying env from: $util"
        copy_with_deps "$util" "$ROOTFS"
        break
    fi
done
for util in /usr/bin/taskset /bin/taskset; do
    if [[ -f "$util" ]]; then
        echo "Copying taskset from: $util"
        copy_with_deps "$util" "$ROOTFS"
        break
    fi
done



# Create LD_PRELOAD environment variable
if [[ ${#PRELOAD_PATHS[@]} -gt 0 ]]; then
    LD_PRELOAD_VALUE=$(IFS=:; echo "${PRELOAD_PATHS[*]}")
    echo "LD_PRELOAD will be set to: $LD_PRELOAD_VALUE"
fi

# Make rootfs accessible to user
sudo chown -R $(id -u):$(id -g) "$ROOTFS"

# Debug: Show what's in the rootfs
echo "Contents of $ROOTFS/bin/:"
ls -la "$ROOTFS/bin/" || echo "No /bin directory found"

echo "Program basename: $PROG_BASENAME"
echo "Checking if program exists in rootfs:"
if [[ -f "$ROOTFS/bin/$PROG_BASENAME" ]]; then
    echo "✓ Program found at $ROOTFS/bin/$PROG_BASENAME"
    file "$ROOTFS/bin/$PROG_BASENAME"
else
    echo "✗ Program NOT found at $ROOTFS/bin/$PROG_BASENAME"
    echo "Available files in $ROOTFS/bin/:"
    ls -la "$ROOTFS/bin/" 2>/dev/null || echo "Directory is empty or doesn't exist"
fi

# Debug: Show preload libraries in rootfs
if [[ ${#PRELOAD_PATHS[@]} -gt 0 ]]; then
    echo "Checking preload libraries in rootfs:"
    for lib_path in "${PRELOAD_PATHS[@]}"; do
        rootfs_lib_path="$ROOTFS$lib_path"
        if [[ -f "$rootfs_lib_path" ]]; then
            echo "✓ $lib_path → exists in rootfs"
        else
            echo "✗ $lib_path → NOT found in rootfs at $rootfs_lib_path"
        fi
    done
fi

echo "Setting up DAX devices..."
for dax_dev in /dev/dax0.0 /dev/dax1.0; do
    if [[ -e "$dax_dev" ]]; then
        echo "Found $dax_dev, will bind mount into container"
        sudo touch "$ROOTFS$dax_dev"  # Create the mount point
    else
        echo "Warning: $dax_dev not found on host"
    fi
done

if [[ -e /dev/dax0.0 ]] && [[ -e $ROOTFS/dev/dax0.0 ]]; then
    sudo mount --bind /dev/dax0.0 $ROOTFS/dev/dax0.0
    echo \"Mounted /dev/dax0.0 into container\"
fi
if [[ -e /dev/dax1.0 ]] && [[ -e $ROOTFS/dev/dax1.0 ]]; then
    sudo mount --bind /dev/dax1.0 $ROOTFS/dev/dax1.0
    echo \"Mounted /dev/dax1.0 into container\"
fi


# for dax_dev in /dev/dax0.0 /dev/dax1.0; do
#     if [[ -e \"\$dax_dev\" ]] && [[ -e \"$ROOTFS\$dax_dev\" ]]; then
#         mount --bind \"\$dax_dev\" \"$ROOTFS\$dax_dev\"
#         echo \"Mounted \$dax_dev into container\"
#     fi
# done

echo "Starting containerized program..."

# Run with user systemd
if [[ ${#PRELOAD_PATHS[@]} -gt 0 ]]; then
       # With LD_PRELOAD
    systemd-run --user --scope \
        --property=MemoryMax=2G \
        --property=CPUQuota=50% \
        bash -c "
            sudo unshare --mount --pid --fork --mount-proc='$ROOTFS/proc' \
            bash -c '
                # Bind mount DAX devices
                for dax_dev in /dev/dax0.0 /dev/dax1.0; do
                    if [[ -e \"\$dax_dev\" ]] && [[ -e \"$ROOTFS\$dax_dev\" ]]; then
                        mount --bind \"\$dax_dev\" \"$ROOTFS\$dax_dev\"
                        echo \"Mounted \$dax_dev into container\"
                    fi
                done
                
                chroot \"$ROOTFS\" taskset -c 8 env LD_PRELOAD=\"$LD_PRELOAD_VALUE\" \"/bin/$PROG_BASENAME\" ${ARGS[*]}
            '
        "
    # # With LD_PRELOAD
    # systemd-run --user --scope \
    #     --property=MemoryMax=2G \
    #     --property=CPUQuota=50% \
    #     bash -c "
    #         sudo unshare --mount --pid --fork --mount-proc='$ROOTFS/proc' \
    #         chroot '$ROOTFS' env LD_PRELOAD='$LD_PRELOAD_VALUE' '/bin/$PROG_BASENAME' ${ARGS[*]}
    #     "
else
    # Without LD_PRELOAD
    systemd-run --user --scope \
        --property=MemoryMax=2G \
        --property=CPUQuota=50% \
        bash -c "
            sudo unshare --mount --pid --fork --mount-proc='$ROOTFS/proc' \
            chroot '$ROOTFS' '/bin/$PROG_BASENAME' ${ARGS[*]}
        "
fi

# Cleanup
echo "Cleaning up..."
sudo rm -rf "$ROOTFS"
