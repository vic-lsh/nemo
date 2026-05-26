#!/bin/bash

# Set LM_LICENSE_FILE in your environment (or a shell rc) to point at your
# Questa/ModelSim license file before running this script.
: "${LM_LICENSE_FILE:?LM_LICENSE_FILE must be set to your Questa license file path}"
export LM_LICENSE_FILE

vsim -do view_sim.do -view vsim.wlf

