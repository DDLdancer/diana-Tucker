#!/bin/bash
module purge
module load cmake/3.16.0
export CC=/gpfs/share/home/1900017781/intel/oneapi/compiler/latest/linux/bin/icx
export CXX=/gpfs/share/home/1900017781/intel/oneapi/compiler/latest/linux/bin/icpx
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/gpfs/share/home/1900017781/intel/oneapi/compiler/latest/linux/lib/

source ~/intel/oneapi/setvars.sh
