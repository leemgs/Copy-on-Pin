#!/bin/bash

INSTALL=/usr/cpu2017
ITERATIONS=3

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root"
    exit 1
fi

if [ ! -f "$INSTALL/shrc" ]; then
    echo "SPEC CPU 2017 installation not found at $INSTALL"
    exit 1
fi

if [ ! -f "$INSTALL/config/cop-paper.cfg" ]; then
    echo "cop-paper.cfg not found in $INSTALL/config/"
    exit 1
fi

run_spec() {
        OUTPUT=$1

        pushd $INSTALL

        # remove previous results
        rm -rf result/*
        rm -rf tmp/*

        # perform a reportable run.
        source shrc
        runcpu --config=cop-paper --reportable --iterations=$ITERATIONS -o csv,txt intrate fprate

        # move the results
        mv result/* $OUTPUT

        popd
}

KERNEL=`uname -r`
if [ -d "results/specrate2017/$KERNEL" ]; then
        echo "results/specrate2017/$KERNEL already exist"
        exit 1
fi
mkdir -p results/specrate2017/$KERNEL
mkdir -p results/specrate2017/$KERNEL/nothp
mkdir -p results/specrate2017/$KERNEL/thp

echo " ... with THP=never"
echo never > /sys/kernel/mm/transparent_hugepage/enabled
run_spec `pwd`/results/specrate2017/$KERNEL/nothp/

echo " ... with THP=always"
echo always > /sys/kernel/mm/transparent_hugepage/enabled
run_spec `pwd`/results/specrate2017/$KERNEL/thp/
