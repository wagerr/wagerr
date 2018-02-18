# dependencies to cross compile (arm64)
sudo apt-get update
sudo apt-get install curl g++-aarch64-linux-gnu g++-4.8-aarch64-linux-gnu gcc-4.8-aarch64-linux-gnu binutils-aarch64-linux-gnu g++-arm-linux-gnueabihf g++-4.8-arm-linux-gnueabihf gcc-4.8-arm-linux-gnueabihf binutils-arm-linux-gnueabihf g++-4.8-multilib gcc-4.8-multilib binutils-gold bsdmainutils
cd ../../depends

make HOST=aarch64-linux-gnu NO_WALLET=1
cd ..
./autogen.sh
./configure --prefix=`pwd`/depends/aarch64-linux-gnu
make HOST=aarch64-linux-gnu
