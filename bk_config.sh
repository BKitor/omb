#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

set -e

# source initenv.bash

./configure CC=$(which mpicc) CXX=$(which mpicxx) \
	--prefix="$BK_OMB_DIR/build" 
	# --enable-cuda 

make -j 16
make install