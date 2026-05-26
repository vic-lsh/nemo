#!/bin/bash

cd intel_rtile_cxl_top_0_ed/hardware_test_design/

quartus_pfg -c ./sof_to_pof.pfg

if [ $? -eq 0 ]; then
    echo "Conversion successful."
else
    echo "Conversion failed!"
    exit 1
fi
