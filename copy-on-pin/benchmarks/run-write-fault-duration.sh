#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# make sure it is compiled
make -C write-fault-duration > /dev/null || { echo 'compilation failed' ; exit 1; }

echo never > /sys/kernel/mm/transparent_hugepage/enabled
KERNEL=`uname -r`

if [ -d "results/write-fault-duration/$KERNEL" ]; then
    echo "Results already exist"
    exit 1
fi
mkdir -p results/write-fault-duration/$KERNEL


./write-fault-duration/write-fault-duration 100 > results/write-fault-duration/$KERNEL/results.csv

#echo 2 > /sys/kernel/mm/ksm/run
