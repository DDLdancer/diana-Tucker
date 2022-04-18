#!/bin/bash
#SBATCH -o output/job.%j.out
#SBATCH -p C032M0128G
#SBATCH --qos=high
#SBATCH --time=00:10:00
#SBATCH -J diana-tucker
#SBATCH --nodes=16
#SBATCH --ntasks-per-node=1

module purge
module load gcc/9.3.0
module load openmpi/3.1.4-gcc-4.8.5
module load lapack/3.9.0-gcc-4.8.5
module load mkl/2017.1

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/gpfs/share/software/gcc/9.3.0/lib64

mpiexec -n 16 diana-tucker input.txt