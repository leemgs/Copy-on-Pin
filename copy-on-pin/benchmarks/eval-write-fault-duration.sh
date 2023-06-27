#!/bin/bash

DIR=`pwd`

if [ ! -d "results/write-fault-duration/" ]; then
	echo "No results available"
	exit 1
fi

cd results/write-fault-duration/
echo "kernel,ns,ns rsd" > $DIR/results/write-fault-duration.csv
for K in *; do
	NUMBERS=`tail -n +2 $K/results.csv | cut -d"," -f1`
	ns_sd=$(
	    echo "$NUMBERS" |
	        awk '{sum+=$1; sumsq+=$1*$1}END{printf "%.6f", sqrt(sumsq/NR - (sum/NR)**2) }'
	)
	ns_average=$(
	    echo "$NUMBERS" |
	        awk '{ total += $1; count++ } END { printf "%.6f", total/count }'
	)
	ns_rsd=`echo "scale=6 ; $ns_sd / $ns_average" | bc`

	echo "$K,$ns_average,$ns_rsd" >> $DIR/results/write-fault-duration.csv
done
