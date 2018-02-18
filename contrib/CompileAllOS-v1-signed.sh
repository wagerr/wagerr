
cd depends;
make download;make download-osx;make download-win;make download-linux;
make HOST=x86_64-pc-linux-gnu;
make HOST=i686-pc-linux-gnu;
make HOST=x86_64-w64-mingw32;
make HOST=i686-w64-mingw32;
make HOST=aarch64-linux-gnu;
make HOST=arm-linux-gnueabihf;
make HOST=x86_64-apple-darwin11;
make HOST=host-platform-triplet;

cd ..;
# x86_64-pc-linux-gnu
./autogen.sh;
./configure --prefix=`pwd`/depends/x86_64-pc-linux-gnu
make
mkdir -p build/v0.0.0.1/x86_64-pc-linux-gnu;
cp ./src/wagerrd ./build/v0.0.0.1/x86_64-pc-linux-gnu/wagerrd;
cp ./src/qt/wagerr-qt ./build/v0.0.0.1/x86_64-pc-linux-gnu/wagerr-qt;
strip ./build/v0.0.0.1/x86_64-pc-linux-gnu/wagerrd
strip ./build/v0.0.0.1/x86_64-pc-linux-gnu/wagerr-qt
## created detached signatures
cd build/v0.0.0.1/x86_64-pc-linux-gnu;

gpg --detach-sign -o wagerr-qt.sig wagerr-qt
gpg --verify wagerr-qt.sig

gpg --armor --detach-sign -o wagerrd.sig wagerrd
gpg --verify wagerrd.sig
cd ../../..;

make clean;cd src;make clean;cd ..;

# i686-pc-linux-gnu
./autogen.sh;
./configure --prefix=`pwd`/depends/i686-pc-linux-gnu
make
mkdir -p build/v0.0.0.1/i686-pc-linux-gnu;
cp ./src/wagerrd ./build/v0.0.0.1/i686-pc-linux-gnu/wagerrd;
cp ./src/qt/wagerr-qt ./build/v0.0.0.1/i686-pc-linux-gnu/wagerr-qt;
strip ./build/v0.0.0.1/i686-pc-linux-gnu/wagerrd
strip ./build/v0.0.0.1/i686-pc-linux-gnu/wagerr-qt
# created detached signatures
cd build/v0.0.0.1/i686-pc-linux-gnu;

gpg --detach-sign -o wagerr-qt.sig wagerr-qt
gpg --verify wagerr-qt.sig

gpg --armor --detach-sign -o wagerrd.sig wagerrd
gpg --verify wagerrd.sig
cd ../../..;

make clean;cd src;make clean;cd ..;

# x86_64-w64-mingw32
./autogen.sh;
./configure --prefix=`pwd`/depends/x86_64-w64-mingw32
make HOST=x86_64-w64-mingw32

mkdir -p build/v0.0.0.1/x86_64-w64-mingw32;
cp ./src/wagerrd.exe ./build/v0.0.0.1/x86_64-w64-mingw32/wagerrd.exe;
cp ./src/qt/wagerr-qt.exe ./build/v0.0.0.1/x86_64-w64-mingw32/wagerr-qt.exe;
strip ./build/v0.0.0.1/x86_64-w64-mingw32/wagerrd.exe
strip ./build/v0.0.0.1/x86_64-w64-mingw32/wagerr-qt.exe
## created detached signatures
cd build/v0.0.0.1/x86_64-w64-mingw32;


##/C= 	Country 	GB
##/ST= 	State 	London
##/L= 	Location 	London
##/O= 	Organization 	Global Security
##/OU= 	Organizational Unit 	IT Department
##/CN= 	Common Name 	example.com

openssl req -x509 -nodes -days 365 -newkey rsa:4096 -keyout ./wagerr-qt-selfsigned.key -out ./wagerr-qt-selfsigned.crt -subj "/C=AT/ST=Vienna/L=Vienna/O=Development/OU=Core Development/CN=ewagerrcore.com";
openssl req -x509 -nodes -days 365 -newkey rsa:4096 -keyout ./wagerrd-selfsigned.key -out ./wagerrd-selfsigned.crt -subj "/C=AT/ST=Vienna/L=Vienna/O=Development/OU=Core Development/CN=ewagerrcore.com";

osslsigncode sign -certs wagerr-qt-selfsigned.crt -key wagerr-qt-selfsigned.key \
        -n "Wagerr Core source code" -i http://www.wagerrcore.com/ \
        -t http://timestamp.verisign.com/scripts/timstamp.dll \
        -in wagerr-qt.exe -out wagerr-qt-signed.exe

osslsigncode sign -certs wagerrd-selfsigned.crt -key wagerrd-selfsigned.key \
        -n "Wagerr Core source code" -i http://www.wagerrcore.com/ \
        -t http://timestamp.verisign.com/scripts/timstamp.dll \
        -in wagerrd.exe -out wagerrd-signed.exe

mv wagerr-qt-signed.exe wagerr-qt.exe;
mv wagerrd-signed.exe wagerrd.exe;

cd ../../..;
make clean;cd src;make clean;cd ..;

# i686-w64-mingw32
./autogen.sh;
./configure --prefix=`pwd`/depends/i686-w64-mingw32
make HOST=i686-w64-mingw32

mkdir -p build/v0.0.0.1/i686-w64-mingw32;
cp ./src/wagerrd.exe ./build/v0.0.0.1/i686-w64-mingw32/wagerrd.exe;
cp ./src/qt/wagerr-qt.exe ./build/v0.0.0.1/i686-w64-mingw32/wagerr-qt.exe;
strip ./build/v0.0.0.1/i686-w64-mingw32/wagerrd.exe
strip ./build/v0.0.0.1/i686-w64-mingw32/wagerr-qt.exe
## created detached signatures
cd build/v0.0.0.1/i686-w64-mingw32;

##/C= 	Country 	GB
##/ST= 	State 	London
##/L= 	Location 	London
##/O= 	Organization 	Global Security
##/OU= 	Organizational Unit 	IT Department
##/CN= 	Common Name 	example.com

openssl req -x509 -nodes -days 365 -newkey rsa:4096 -keyout ./wagerr-qt-selfsigned.key -out ./wagerr-qt-selfsigned.crt -subj "/C=AT/ST=Vienna/L=Vienna/O=Development/OU=Core Development/CN=ewagerrcore.com";
openssl req -x509 -nodes -days 365 -newkey rsa:4096 -keyout ./wagerrd-selfsigned.key -out ./wagerrd-selfsigned.crt -subj "/C=AT/ST=Vienna/L=Vienna/O=Development/OU=Core Development/CN=ewagerrcore.com";

osslsigncode sign -certs wagerr-qt-selfsigned.crt -key wagerr-qt-selfsigned.key \
        -n "Wagerr Core source code" -i http://www.wagerrcore.com/ \
        -t http://timestamp.verisign.com/scripts/timstamp.dll \
        -in wagerr-qt.exe -out wagerr-qt-signed.exe

osslsigncode sign -certs wagerrd-selfsigned.crt -key wagerrd-selfsigned.key \
        -n "Wagerr Core source code" -i http://www.wagerrcore.com/ \
        -t http://timestamp.verisign.com/scripts/timstamp.dll \
        -in wagerrd.exe -out wagerrd-signed.exe

mv wagerr-qt-signed.exe wagerr-qt.exe;
mv wagerrd-signed.exe wagerrd.exe;

cd ../../..;
make clean;cd src;make clean;cd ..;

./autogen.sh;
./configure --prefix=`pwd`/depends/arm-linux-gnueabihf
make HOST=arm-linux-gnueabihf;

mkdir -p build/v0.0.0.1/arm-linux-gnueabihf;
cp ./src/wagerrd ./build/v0.0.0.1/arm-linux-gnueabihf/wagerrd;
cp ./src/qt/wagerr-qt ./build/v0.0.0.1/arm-linux-gnueabihf/wagerr-qt;
strip ./build/v0.0.0.1/arm-linux-gnueabihf/wagerrd
strip ./build/v0.0.0.1/arm-linux-gnueabihf/wagerr-qt
# created detached signatures
cd build/v0.0.0.1/arm-linux-gnueabihf;

gpg --detach-sign -o wagerr-qt.sig wagerr-qt
gpg --verify wagerr-qt.sig

gpg --armor --detach-sign -o wagerrd.sig wagerrd
gpg --verify wagerrd.sig
cd ../../..;


make clean;cd src;make clean;cd ..;

# aarch64-linux-gnu
./autogen.sh;
./configure --prefix=`pwd`/depends/aarch64-linux-gnu
make HOST=aarch64-linux-gnu;

mkdir -p build/v0.0.0.1/aarch64-linux-gnu;
cp ./src/wagerrd ./build/v0.0.0.1/aarch64-linux-gnu/wagerrd;
cp ./src/qt/wagerr-qt ./build/v0.0.0.1/aarch64-linux-gnu/wagerr-qt;
strip ./build/v0.0.0.1/aarch64-linux-gnu/wagerrd
strip ./build/v0.0.0.1/aarch64-linux-gnu/wagerr-qt
# created detached signatures
cd build/v0.0.0.1/aarch64-linux-gnu;

gpg --detach-sign -o wagerr-qt.sig wagerr-qt
gpg --verify wagerr-qt.sig

gpg --armor --detach-sign -o wagerrd.sig wagerrd
gpg --verify wagerrd.sig
cd ../../..;

# arm-linux-gnueabihf
./autogen.sh;
./configure --prefix=`pwd`/depends/arm-linux-gnueabihf
make
mkdir -p build/v0.0.0.1/arm-linux-gnueabihf;
cp ./src/wagerrd ./build/v0.0.0.1/arm-linux-gnueabihf/wagerrd;
cp ./src/qt/wagerr-qt ./build/v0.0.0.1/arm-linux-gnueabihf/wagerr-qt;
strip ./build/v0.0.0.1/arm-linux-gnueabihf/wagerrd
strip ./build/v0.0.0.1/arm-linux-gnueabihf/wagerr-qt
## created detached signatures
cd build/v0.0.0.1/arm-linux-gnueabihf;

gpg --detach-sign -o wagerr-qt.sig wagerr-qt
gpg --verify wagerr-qt.sig

gpg --armor --detach-sign -o wagerrd.sig wagerrd
gpg --verify wagerrd.sig
cd ../../..;

make clean;cd src;make clean;cd ..;

# host-platform-triplet
./autogen.sh;
./configure --prefix=`pwd`/depends/host-platform-triplet
make
mkdir -p build/v0.0.0.1/host-platform-triplet;
cp ./src/wagerrd ./build/v0.0.0.1/host-platform-triplet/wagerrd;
cp ./src/qt/wagerr-qt ./build/v0.0.0.1/host-platform-triplet/wagerr-qt;
strip ./build/v0.0.0.1/host-platform-triplet/wagerrd
strip ./build/v0.0.0.1/host-platform-triplet/wagerr-qt
## created detached signatures
cd build/v0.0.0.1/host-platform-triplet;

gpg --detach-sign -o wagerr-qt.sig wagerr-qt
gpg --verify wagerr-qt.sig

gpg --armor --detach-sign -o wagerrd.sig wagerrd
gpg --verify wagerrd.sig
cd ../../..;

make clean;cd src;make clean;cd ..;

# x86_64-apple-darwin11
./autogen.sh;
./configure --prefix=`pwd`/depends/x86_64-apple-darwin11
make HOST=x86_64-apple-darwin11;

#mkdir -p build/v0.0.0.1/x86_64-apple-darwin11;
#cp ./src/wagerrd ./build/v0.0.0.1/x86_64-apple-darwin11/wagerrd;
#cp ./src/qt/wagerr-qt ./build/v0.0.0.1/x86_64-apple-darwin11/wagerr-qt;
#strip ./build/v0.0.0.1/x86_64-apple-darwin11/wagerrd
#strip ./build/v0.0.0.1/x86_64-apple-darwin11/wagerr-qt
# created detached signatures
#cd build/v0.0.0.1/x86_64-apple-darwin11;

#gpg --detach-sign -o wagerr-qt.sig wagerr-qt
#gpg --verify wagerr-qt.sig

#gpg --armor --detach-sign -o wagerrd.sig wagerrd
#gpg --verify wagerrd.sig
#cd ../../..;


#make clean;cd src;make clean;cd ..;