#!/bin/bash

cat occupancy \
	| sed -e '1,3d' \
	| awk 'BEGIN {total=0} {total+=$3} END {printf("%.2f%%\n",total/NR)}'
