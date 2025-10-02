#!/bin/bash

OUTCSV="stride.csv"

M=5000000          
REPEATS=10        
KERNEL="saxpy"
DTYPE="f32"     

STRIDES=(1 2 4 8)

rm -f $OUTCSV

echo "=== Running scalar build ==="
for s in "${STRIDES[@]}"; do
    ./kernel_scalar --kernel $KERNEL --dtype $DTYPE --M $M --stride $s --repeats $REPEATS --csv $OUTCSV
done

echo "=== Running simd build ==="
for s in "${STRIDES[@]}"; do
    ./kernel_simd --kernel $KERNEL --dtype $DTYPE --M $M --stride $s --repeats $REPEATS --csv $OUTCSV
done

echo "All experiments finished. Results saved in $OUTCSV"
