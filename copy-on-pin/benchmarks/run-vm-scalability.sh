#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# make sure it is compiled
make -C vm-scalability > /dev/null || { echo 'compilation failed' ; exit 1; }

ANON="case-anon-cow-seq case-anon-cow-rand"

CASES="$ANON"
KERNEL=`uname -r`

run_case() {
	CASE=$1
	DIR=$2

	sync; echo 3 > /proc/sys/vm/drop_caches

	pushd vm-scalability

	# warmup
	./usemem --runtime 3000 -n 12 --prealloc --prefault 2147483648 > /dev/null

	for t in $(seq 2 2 12); do
		echo " ... with $t tasks"

		echo "Bytes,Usecs,KBs" >> $DIR/$c-$t.csv
	
		for i in $(seq 1 1 50); do
			echo " ... run $i"

			export nr_task=$t
			export unit_size=$((1024*1024*1024*t))
	
			bash -c "./$c" > $DIR/$c-tmp
			ret=$?
			if [ $ret -ne 0 ]; then
				echo "$c failed with $ret"
			fi
	
			bytes_total=0
			usecs_total=0
			kbs_total=0
			while read -r line; do
				bytes=`echo $line | cut -d" " -f1`
				usecs=`echo $line | cut -d" " -f4`
				kbs=`echo $line | cut -d" " -f7`
				bytes_total=$((bytes_total+bytes))
				usecs_total=$((usecs_total+usecs))
				kbs_total=$((kbs_total+kbs))
			done < $DIR/$c-tmp
			rm $DIR/$c-tmp

			echo "$bytes_total,$usecs_total,$kbs_total" >> $DIR/$c-$t.csv
			echo "$bytes_total,$usecs_total,$kbs_total"
		done
	done

	popd
}

if [ -d "results/vm-scalability/$KERNEL" ]; then
	echo "results/vm-scalability already exist"
	exit 1
fi
mkdir -p results/vm-scalability/$KERNEL
mkdir -p results/vm-scalability/$KERNEL/nothp
mkdir -p results/vm-scalability/$KERNEL/thp

for c in $CASES
do
	echo "Processing $c"

	echo " ... with THP=never"
	echo never > /sys/kernel/mm/transparent_hugepage/enabled
	run_case $c `pwd`/results/vm-scalability/$KERNEL/nothp

	echo " ... with THP=always"
	echo always > /sys/kernel/mm/transparent_hugepage/enabled
	run_case $c `pwd`/results/vm-scalability/$KERNEL/thp
done
