#!/bin/bash
set -e

if ! command -v vsim &> /dev/null; then
    echo "Error: 'vsim' command not found. 🤷" >&2
    echo "Please make sure that your Questa installation is in your \$PATH" >&2
    exit 1
fi


if [ ! -f "$LM_LICENSE_FILE" ]; then
    echo "Error: Please set up an environment variable for the questa license file." >&2
    exit 1
fi

if [ -z "$1" ]; then
    echo "Usage: $0 <tb_file>"
    exit 1
fi
tb="$1"

python3 "$tb" intel_rtile_cxl_top_0_ed/hardware_test_design/sim/afu_top_tb_controlflow_generated.sv

vsim -c -do run_sim.do
