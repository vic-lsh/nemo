#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

quartus_sh --flow compile $SCRIPT_DIR/../intel_rtile_cxl_top_0_ed/hardware_test_design/cxltyp3_memexp_ddr4_top.qpf -end dni_elaboration
