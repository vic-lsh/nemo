# Create working library
vlib work
vmap work work

vlib altera_modules
vmap altera_modules altera_modules

# Compile library files
if {![info exists env(QUARTUS_ROOTDIR)]} {
    error "QUARTUS_ROOTDIR is not set. Source your Quartus environment before running simulation."
}
set lib_filelist [open "lib_filelist.txt" r]
while {[gets $lib_filelist line] >= 0} {
    # Skip empty lines and comments
    if {[string length $line] == 0 || [string index $line 0] == "#"} {
        continue
    }

    # Expand $QUARTUS_ROOTDIR and any other env vars referenced in the line.
    regsub -all {\$QUARTUS_ROOTDIR} $line $env(QUARTUS_ROOTDIR) line

    # Extract file type and compile accordingly
    set extension [file extension $line]
    switch $extension {
        ".v"  {vlog -work altera_modules $line}
        ".sv" {vlog -sv -work altera_modules $line}
        ".vhd" {vcom -work altera_modules $line}
        default {puts "Unknown file type: $line"}
    }
}
close $lib_filelist


# Read filelist and compile each file
set filelist [open "filelist.txt" r]
while {[gets $filelist line] >= 0} {
    # Skip empty lines and comments
    if {[string length $line] == 0 || [string index $line 0] == "#"} {
        continue
    }
    
    # Extract file type and compile accordingly
    set extension [file extension $line]
    switch $extension {
        ".v"  {vlog -work work $line}
        ".sv" {vlog -sv -work work -incdir intel_rtile_cxl_top_0_ed/hardware_test_design/common $line}
        ".vhd" {vcom -work work $line}
        default {puts "Unknown file type: $line"}
    }
}
close $filelist

# Start simulation with top module
#vsim -t 1ps work.axi_demux_mem_controller_tb -L altera_modules
vsim -t 1ps work.afu_top_tb -vopt -voptargs="+acc" -L altera_modules

# Add all signals to wave window
log -r "/*"

view wave
add wave -r "/*"

# Run simulation for 10us
run 1000us

quit
