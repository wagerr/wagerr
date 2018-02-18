# dependencies to cross compile (win64)
sudo apt-get update
sudo apt-get install g++-mingw-w64-x86-64 mingw-w64-x86-64-dev
sudo update-alternatives --config x86_64-w64-mingw32-g++
cd ../../depends

make HOST=x86_64-w64-mingw32 -j4
cd ..
./autogen.sh # not required when building from tarball
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site ./configure --prefix=/
make
make deploy
