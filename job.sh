#!/bin/bash
#SBATCH -o output/job.%j.out
#SBATCH -p C032M0128G
#SBATCH --qos=high
#SBATCH --time=00:10:00
#SBATCH -J diana-tucker
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=4
#SBATCH --mail-type=BEGIN
#SBATCH --mail-user=zhangyc.life@gmail.com

source ./wmenv.sh >> /dev/null
mpirun -np 4 -genv OMP_NUM_THREADS=1 -genv I_MPI_PIN_DOMAIN=omp ./diana-tucker ./input_test_openmp.txt
