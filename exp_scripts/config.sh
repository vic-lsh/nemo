#!/bin/bash

HEMEM_LIB=./build/libhemem.so

# cores before this one are used by hemem
HEMEM_APP_CPU_START=8
# default number of CPU cores to give to an application
APP_CPUS_DEFAULT=4
