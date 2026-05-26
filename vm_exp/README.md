# vm_exp

Minimal setup to run experiments on VMs.

## Getting started

``` bash
# do these once
./mk_disk_img.sh     # download vm image
./mk_cidata.sh       # create login info for the new vm

# start the vm
./vm_start.sh

# login via ssh on another terminal
./vm_login.sh

# Nemo integration
# To start transmitting PML bitmaps, run this script in another terminal
./vm_incoming.sh
```

Currently, the PML integration works by hijacking the VM migration code path
in Qemu: instead of transferring over VM state, the code path simply transfers
the dirty bitmap to the UCM. Therefore, to start transmitting PML bitmaps, we
start the VM migration (via `./vm_incoming.sh`) to execute this code path.
