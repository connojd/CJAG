#!/bin/bash

i="0"

printf "L3 occupancy stats\n" > occupancy
printf "%-8s | %-8s | %-8s | %-8s\n" "cjag1(B)" "cjag1(%)" "cjag2(B)" "cjag2(%)" >> occupancy
printf "%-8s | %-8s | %-8s | %-8s\n" "--------" "--------" "--------" "--------" >> occupancy

while [ $i -eq 0 ]
do 
    c1=`sudo xl psr-cmt-show cache-occupancy 10 | grep cjag1 | awk '{printf $3}'`
    #c2=`sudo xl psr-cmt-show cache-occupancy 11 | grep cjag2 | awk '{printf $3}'`
    c2=0
    c2_pct=0

    c1_pct=$(echo "scale=2; ($c1 / 20480) * 100" | bc)
    #printf "scale=2; (%d / 20480) * 100" "$c2" | bc

    printf "%-8s | %-8s | %-8s | %-8s\n" "$c1" "$c1_pct%" "$c2" "$c2_pct%" >> occupancy
done
