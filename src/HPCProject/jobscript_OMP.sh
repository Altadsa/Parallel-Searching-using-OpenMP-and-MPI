#!/bin/bash

#SBATCH -o output_OMP.out
#SBATCH -e output_OMP.err

#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4

gcc -fopenmp project_OMP.c -o execute_OMP -std=c11

echo "Start Open MP Project"

time ./project_OMP small-inputs

echo "Finish Open MP Project"