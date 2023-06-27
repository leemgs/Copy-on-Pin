#!/bin/bash
CASES="mprotect swap mprotect+ksm fork swap+fork fork+ksm"

if [ ! -d "results/stream/" ]; then
	echo "No results available"
	exit 1
fi

DIR=`pwd`

cd results/stream/
for K in *; do
	echo "action,gb/s,gb/s rsd,copies/s,copies/s rsd" > $DIR/results/$K-stream.csv
	for C in $CASES; do

		NUMBERS=`tail -n +2 $K/$C.csv | cut -d"," -f1`
		gbs_sd=$(
		    echo "$NUMBERS" |
		        awk '{sum+=$1; sumsq+=$1*$1}END{printf "%.6f", sqrt(sumsq/NR - (sum/NR)**2)/1024/1024}'
		)
		gbs_average=$(
		    echo "$NUMBERS" |
		        awk '{ total += $1; count++ } END { printf "%.6f", total/count/1024/1024 }'
		)
		gbs_rsd=`echo "scale=6 ; $gbs_sd / $gbs_average" | bc`

		NUMBERS=`tail -n +2 $K/$C.csv | cut -d"," -f2`
		copies_sd=$(
		    echo "$NUMBERS" |
		        awk '{sum+=$1; sumsq+=$1*$1}END{printf "%.6f", sqrt(sumsq/NR - (sum/NR)**2)/1000}'
		)
		copies_average=$(
		    echo "$NUMBERS" |
		        awk '{ total += $1; count++ } END { printf "%.6f", total/count/1000 }'
		)
		if [ "$copies_average" = "0.000000" ]; then
			copies_rsd=0
		else
			copies_rsd=`echo "scale=6 ; $copies_sd / $copies_average" | bc`
		fi

		echo $C,$gbs_average,$gbs_rsd,$copies_average,$copies_rsd >> $DIR/results/$K-stream.csv
	done
done
