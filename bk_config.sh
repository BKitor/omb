#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

set -e

pushd $HOME/openmpi-5.0.0rc2
source initenv.bash
popd

./configure CC=$(which mpicc) CXX=$(which mpicxx) \
	--prefix=$BK_OMB_DIR \
	--enable-cuda 

make -j 16
make install