#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vic Li");
MODULE_DESCRIPTION("A simple Hello World kernel module");
MODULE_VERSION("0.1");

#define KB (1 << 10)
#define MB (1 << 20)
#define GB (1 << 30)

#define REGION2_BASE_ADDR 0x20beffa00000
#define BYTES_TO_READ (2 * MB)

static void __iomem *mapped_base;

int read_phys_mem(void) {
    unsigned long value;
    unsigned int *ptr;
    int i;

    printk(KERN_INFO "Loading read_phys_mem module\n");

    // Map the physical address to kernel virtual address
    mapped_base = ioremap(REGION2_BASE_ADDR, BYTES_TO_READ);
    if (!mapped_base) {
        printk(KERN_ERR "ioremap failed\n");
        return -ENOMEM;
    }

    // Read from the mapped address
    value = ioread32(mapped_base);
    printk(KERN_INFO "Read value: 0x%lx\n", value);

    ptr = (unsigned int *)mapped_base;
    for (i = 0; i < 64; i++) {
        value = ioread32(ptr + i);
        printk(KERN_INFO "%d 0x%lx\n", i, value);
        // if ((i + 1) % 16 == 0) {
        //     printf("\n");
        // }
    }

    return 0;
}

static int __init memtest_init(void) {
    printk(KERN_INFO "Hello, World!\n");

    read_phys_mem();

    return 0;
}

static void __exit memtest_exit(void) {
    printk(KERN_INFO "Goodbye, World!\n");

    if (mapped_base) {
        printk(KERN_INFO "Unmapping memtest module\n");
        iounmap(mapped_base);
    }
}

module_init(memtest_init);
module_exit(memtest_exit);
