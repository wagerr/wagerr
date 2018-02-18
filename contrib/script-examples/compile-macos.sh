# dependencies to cross compile (arm64)
sudo apt-get update
sudo apt-get install curl librsvg2-bin libtiff-tools bsdmainutils cmake imagemagick libcap-dev libz-dev libbz2-dev python-setuptools
cd ../../depends

make HOST=aarch64-linux-gnu NO_WALLET=1
cd ..
./autogen.sh
./configure --prefix=`pwd`/depends/aarch64-linux-gnu
make HOST=aarch64-linux-gnu
