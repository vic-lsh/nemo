#!/bin/bash

# util script to wrap any application script with nemo interposition.

source ./exp_scripts/config.sh

echo "All arguments with \$@: $@"

USE_GDB=false
USE_NEMO=true
START_CPU=$HEMEM_APP_CPU_START
CPU_COUNT=$APP_CPUS_DEFAULT
MISS_RATIO=0.1

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --gdb)
            USE_GDB=true
            echo "GDB flag detected. Will run with GDB."
            shift
            ;;
        --nonemo)
            USE_NEMO=false
            echo "No-nemo flag detected. Will run without Nemo."
            shift
            ;;
        --cpus)
            if [[ -n "$2" && "$2" != --* ]]; then
                if [[ "$2" =~ ^[0-9]+$ ]]; then
                    CPU_COUNT=$2
                    echo "CPU count set to: $CPU_COUNT"
                    shift 2
                else
                    echo "Error: --cpus requires a numeric value." >&2
                    exit 1
                fi
            else
                echo "Error: --cpus requires a value." >&2
                exit 1
            fi
            ;;
        --miss-ratio)
            if [[ -n "$2" && "$2" != --* ]]; then
                # allow 0, 1, ints, floats
                if [[ "$2" =~ ^[0-9]*\.?[0-9]+$ ]]; then
                    ratio_val="$2"

                    # compare using bc
                    if (( $(echo "$ratio_val >= 0.0" | bc -l) )) && \
                       (( $(echo "$ratio_val <= 1.0" | bc -l) )); then
                        MISS_RATIO="$ratio_val"
                        echo "Miss ratio set to: $MISS_RATIO"
                        shift 2
                    else
                        echo "Error: --miss-ratio must be between 0.0 and 1.0." >&2
                        exit 1
                    fi
                else
                    echo "Error: --miss-ratio requires a numeric value." >&2
                    exit 1
                fi
            else
                echo "Error: --miss-ratio requires a value." >&2
                exit 1
            fi
            ;;
        *)
            USER_COMMAND="$@"
            break
            ;;
    esac
done

END_CPU=$((START_CPU + CPU_COUNT - 1))

echo ""
echo "--- Configuration ---"
echo "Run with GDB:     $USE_GDB"
echo "Run with Nemo:    $USE_NEMO"
echo "Start CPU:        $START_CPU"
echo "End CPU:          $END_CPU"
echo "Miss Ratio:       $MISS_RATIO"
echo "App command:      $USER_COMMAND"
echo "---------------------"
echo ""

gdb_prelude=
if [ "$USE_GDB" = true ]; then
    gdb_prelude="gdb --args "
fi

nemo_prelude="env LD_PRELOAD=$HEMEM_LIB MISS_RATIO=$MISS_RATIO"
if [ "$USE_NEMO" = false ]; then
    nemo_prelude=""
fi

if [ "$USE_NEMO" = true ]; then
    if [[ ! -f "$HEMEM_LIB" ]]; then
        echo "ERROR: $HEMEM_LIB not found."
        exit 1
    fi
fi

sudo taskset -c $START_CPU-$END_CPU \
    $gdb_prelude \
    $nemo_prelude \
    $USER_COMMAND
