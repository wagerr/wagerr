# dependencies to cross compile (win32)
sudo apt-get update
sudo apt-get install g++-mingw-w64-i686 mingw-w64-i686-dev
sudo update-alternatives --config i686-w64-mingw32-g++  # Set the default mingw32 g++ compiler option to posix.
cd ../../depends


make HOST=i686-w64-mingw32 -j4
cd ..
./autogen.sh # not required when building from tarball
CONFIG_SITE=$PWD/depends/i686-w64-mingw32/share/config.site ./configure --prefix=/
make
make deploy
