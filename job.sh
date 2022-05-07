#!/bin/bash
#SBATCH --output=output/job.%j.out
#SBATCH --partition=C032M0128G
#SBATCH --qos=high
#SBATCH --time=00:10:00
#SBATCH --job-name=diana-tucker
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=4
#SBATCH --mail-type=BEGIN
#SBATCH --mail-user=zhangyc.life@gmail.com

source ./wmenv.sh >> /dev/null
mpirun -np 4 -genv OMP_NUM_THREADS=4 -genv I_MPI_PIN_DOMAIN=omp ./diana-tucker ./input_test_openmp.txt
