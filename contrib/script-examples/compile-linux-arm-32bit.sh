# download latest master
git clone 
# dependencies to cross compile (arm32)
sudo apt-get update
sudo apt-get install curl g++-aarch64-linux-gnu g++-4.8-aarch64-linux-gnu gcc-4.8-aarch64-linux-gnu binutils-aarch64-linux-gnu g++-arm-linux-gnueabihf g++-4.8-arm-linux-gnueabihf gcc-4.8-arm-linux-gnueabihf binutils-arm-linux-gnueabihf g++-4.8-multilib gcc-4.8-multilib binutils-gold bsdmainutils

cd ../../depends

make HOST=arm-linux-gnueabihf NO_WALLET=1
cd ..
./autogen.sh
./configure --prefix=`pwd`/depends/arm-linux-gnueabihf
make HOST=arm-linux-gnueabihf
