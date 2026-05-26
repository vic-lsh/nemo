# Set up CXL memory as a DAX device

By default, the CXL memory on our FPGA is configured as a CPU-less NUMA node.
This guide describes how to configure the CXL memory as a DAX device instead of
a NUMA node.

## 1. Verify CXL memory is a CPU-less NUMA node

First, observe the CXL memory is configured as a CPU-less NUMA node:

```bash
$ numactl -H
node 0 cpus: [omitted]
node 0 size: 257638 MB
node 0 free: 256421 MB
node 1 cpus:
node 1 size: 16123 MB
node 1 free: 15848 MB
node distances:
node   0   1
  0:  10  14
  1:  14  10
```

From the output above, we see that the CXL memory is a 16GiB region on NUMA
node 1.

## 2. Get CXL memory address range

Next, we obtain the memory address range of the NUMA node 1 by inspecting dmesg
output:

```bash
$ sudo dmesg | grep -i node
// .... omitted ....
[    0.019702]   node   1: [mem 0x0000004080000000-0x000000447fffffff]
// .... omitted ....
```

We can see the starting address of node 1 is `0x4080000000` on our server.
Also, the address range fits 16GiB, consistent with the output from `numactl`.

## 3. Reserve CXL memory

The next step prevents setting up the CXL NUMA node at boot time. We do so by
reserving the CXL memory region (and thereby preventing linux from using it).

Append the following parameters to the system GRUB file:

```bash
memmap=<SIZE>!<ADDR>
```

For instance, on our server this would be `memmap=16G!<xxx>`.

Our server cluster's GRUB configs are listed on the `dock` server. See our
server description Google Doc for more detail.

Apply the changes (e.g., `update-grub`) and restart the server.

## 4. Verify CXL memory is not on NUMA

After the server reboots, verify that CXL memory is no longer listed as a
CPU-less NUMA node:

```bash
$ numactl -H
available: 1 nodes (0)
node 0 cpus: [omitted]
node 0 size: 257442 MB
node 0 free: 248622 MB
node distances:
node   0
  0:  10
```

You should also see a new device `pmem0` that points to this reserved memory
region:

```bash
$ ls /dev/pmem0
pmem0

$ ndctl list --human
{
  "dev":"namespace0.0",
  "mode":"fsdax",
  "map":"mem",
  "size":"16.00 GiB (17.18 GB)",
  "sector_size":512,
  "blockdev":"pmem0"
}

$ fdisk -l /dev/pmem0
Disk /dev/pmem0: 16 GiB, 17179869184 bytes, 33554432 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 4096 bytes
I/O size (minimum/optimal): 4096 bytes / 4096 bytes
```

## 5. Set up CXL memory as a DAX device

Note: this step is the same as setting DRAM or PMem as a DAX device.

Change the namespace of `pmem0` from `fsdax` over to devdax mode using the
following command (in this example, the DRAM namespace is called namespace0.0):

```bash
sudo ndctl create-namespace -f -e namespace0.0 --mode=devdax --align 2M
```

Verify that a new dax device has been created:

```bash
$ ls /dev/dax0.0 # or whichever chardev name was returned by ndctl
dax0.0
```

Now you should be able to access CXL memory via this DAX device.
