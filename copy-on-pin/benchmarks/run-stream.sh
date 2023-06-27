#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# make sure it is compiled
make -C stream > /dev/null || { echo 'compilation failed' ; exit 1; }

echo 999999999 > /sys/kernel/mm/ksm/pages_to_scan
echo 0 > /sys/kernel/mm/ksm/sleep_millisecs
echo never > /sys/kernel/mm/transparent_hugepage/enabled
KERNEL=`uname -r`

if [ -d "results/stream/$KERNEL" ]; then
    echo "Results already exist"
    exit 1
fi
mkdir -p results/stream/$KERNEL


MODES="fork swap mprotect swap+fork fork+ksm mprotect+ksm"

echo 1 > /sys/kernel/mm/ksm/run

for M in $MODES; do
    echo "Processing $M"

    ./stream/stream 100 $M > results/stream/$KERNEL/$M.csv
done

echo 2 > /sys/kernel/mm/ksm/run
