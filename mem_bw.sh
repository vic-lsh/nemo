#!/bin/bash

faster_pid=$(pgrep pmem_benchmark)
antagonist_pid=$(pgrep antagonist)

if [ -n "$faster_pid" ]; then
	echo "Faster BW"
	sudo pqos -I -p "mbl:$faster_pid;mbr:$faster_pid" -t 1
fi

if [ -n "$antagonist_pid" ]; then
	echo "-------------------"
	echo "Antagonist BW"
	sudo pqos -I -p "mbl:$antagonist_pid;mbr:$antagonist_pid" -t 1
fi
