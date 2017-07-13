#!/bin/bash

cat occupancy \
	| sed -e '1,3d' \
	| awk '{print $3}' \
	| sort \
	| uniq \
	| tail -n 1
