#!/bin/bash

#SBATCH -o output/OMP.out

#SBATCH --ntasks=1
#SBTACH --cpus-per-task=4

gcc -fopenmp project_OMP.c -o execute_OMP -std=c11 -w -O2

export OMP_NUM_THREADS=4

echo "Start Open MP Project"

time ./execute_OMP large-inputs

echo "Finished Open MP Project"

sort -k 1,1n -k 2,2n -k 3,3n result_OMP.txt > sorted_OMP.txt
