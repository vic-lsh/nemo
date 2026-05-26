# fpga_ctl

Directory for utility programs controlling the FPGA.

## Directory structure

Every `*.c` file in the `bin/` directory will be compiled as a binary. To make
a new binary, create a new file under `bin/`. The compiled binary can be found
at `<project-root>/build/`.

You may add source and header files at this directory, and they can be shared
across `fpga_ctl` binaries.

Libraries `libs/cxl-dev` and `libs/util` are automatically linked to all
`fpga_ctl` binaries.
