#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

quartus_sh --flow compile $SCRIPT_DIR/../intel_rtile_cxl_top_0_ed/hardware_test_design/cxltyp3_memexp_ddr4_top.qpf | tee build.log

if grep 'Info (24095): Timing requirements were met' build.log; then
    echo 'Timing was met!'
else
    echo '*** TIMING NOT MET! ***'
    echo '*** TIMING NOT MET! ***'
    echo '*** TIMING NOT MET! ***'
fi
