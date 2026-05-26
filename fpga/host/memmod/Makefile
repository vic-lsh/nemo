# Makefile for building a kernel module

# Module name
obj-m += memtest_module.o

# Default target
all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# Clean target
clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

