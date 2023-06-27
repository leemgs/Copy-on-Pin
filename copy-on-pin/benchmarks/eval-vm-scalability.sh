#!/bin/bash
CASES="case-anon-cow-rand case-anon-cow-seq"
SUBCASES="thp nothp"

if [ ! -d "results/vm-scalability/" ]; then
	echo "No results available"
	exit 1
fi

DIR=`pwd`

cd results/vm-scalability/
for K in *; do
	for C in $CASES; do
		for S in $SUBCASES; do
			FILE=$DIR/results/$K-$C-$S.csv
			echo "tasks,gb/s,gb/s rsd" > $FILE
			for T in $(seq 2 2 12); do
				NUMBERS=`tail -n +2 $K/$S/$C-$T.csv | cut -d"," -f3`
				gbs_sd=$(
				    echo "$NUMBERS" |
				        awk '{sum+=$1; sumsq+=$1*$1}END{printf "%.6f", sqrt(sumsq/NR - (sum/NR)**2)/1024/1024}'
				)
				gbs_average=$(
				    echo "$NUMBERS" |
				        awk '{ total += $1; count++ } END { printf "%.6f", total/count/1024/1024 }'
				)
				gbs_rsd=`echo "scale=6 ; $gbs_sd / $gbs_average" | bc`

				echo "$T,$gbs_average,$gbs_rsd" >> $FILE
			done
		done
	done
done
