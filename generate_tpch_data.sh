#!/bin/bash

#index=(		0		1		2		3		4		5		6		7 			8		9		10	)
#MB=(		0.1		1		10		100		1000	2000	5000	10000		20000	50000	100000	)
#scale=(		0.0001	0.001	0.01	0.1		1.0		2.0		5.0		10.0		20.0	50.0	100.0	)

index=(0       1       2       3       4      )
MB=(   0.1     1       10      100     1000   )
scale=(0.0001  0.001   0.01    0.1     1      )

echo "compile dbgen..."
cd dbgen
make
if [[ $? -ne 0 ]]; then
   echo "check the compilation options in dbgen/Makefile and try again." >&2
      exit 1
fi

for i in $(seq 0 $((${#scale[@]} - 1))); do
   echo "[$((i+1)) / ${#scale[@]}] generate TPC-H SF ${scale[$i]}..."
   mkdir -p ../data/tpch_${MB[$i]}MB
   ./dbgen -s ${scale[$i]} 2>/dev/null
   mv -f *.tbl ../data/tpch_${MB[$i]}MB/
done
