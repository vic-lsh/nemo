#!/bin/bash
set -e

# The .cdf file contains programming configurations.
#
# The config file assumes the .pof output file to be intel_rtile_cxl_top_0_ed/hardware_test_design/output_file.pof.

# Check if quartus_pgm is in the PATH.
if ! command -v quartus_pgm &> /dev/null; then
    echo-e  "${RED}quartus_pgm not in PATH. Add it to PATH and try again.${NC}"
    exit 1
fi

# We use taskset and chrt to benefit from multiple cores even when they are
# isolated from the linux scheduler. This significantly speeds up loading the
# bitstream. Note that we use all but the last core.
sudo taskset -c 0-$((`nproc --all` - 2)) chrt -r 1 $(which quartus_pgm) \
    -c 1 ./intel_rtile_cxl_top_0_ed/hardware_test_design/output_files/cxltyp3_memexp_ddr4_top.cdf

echo "Loaded bitstream"
