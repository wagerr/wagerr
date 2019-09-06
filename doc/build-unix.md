UNIX BUILD NOTES
====================
Some notes on how to build WAGERR in Unix.

Note
---------------------
Always use absolute paths to configure and compile wagerr and the dependencies,
for example, when specifying the path of the dependency:

	../dist/configure --enable-cxx --disable-shared --with-pic --prefix=$BDB_PREFIX

Here BDB_PREFIX must absolute path - it is defined using $(pwd) which ensures
the usage of the absolute path.

To Build
---------------------

```bash
./autogen.sh
./configure
make
make install # optional
```

This will build wagerr-qt as well if the dependencies are met.

Dependencies
---------------------

These dependencies are required:

 Library     | Purpose            | Description
 ------------|--------------------|----------------------
 libssl      | SSL Support        | Secure communications
 libboost    | Boost              | C++ Library
 libevent    | Events             | Asynchronous event notification
 libgmp      | Bignum Arithmetic  | Precision arithmetic

Optional dependencies:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 miniupnpc   | UPnP Support     | Firewall-jumping support
 libdb4.8    | Berkeley DB      | Wallet storage (only needed when wallet enabled)
 qt          | GUI              | GUI toolkit (only needed when GUI enabled)
 protobuf    | Payments in GUI  | Data interchange format used for payment protocol (only needed when GUI enabled)
 libqrencode | QR codes in GUI  | Optional for generating QR codes (only needed when GUI enabled)
 univalue    | Utility          | JSON parsing and encoding (bundled version will be used unless --with-system-univalue passed to configure)
 libzmq3     | ZMQ notification | Optional, allows generating ZMQ notifications (requires ZMQ version >= 4.0.0)

For the versions used in the release, see [release-process.md](release-process.md) under *Fetch and build inputs*.

System requirements
--------------------

C++ compilers are memory-hungry. It is recommended to have at least 1 GB of
memory available when compiling WAGERR Core. With 512MB of memory or less
compilation will take much longer due to swap thrashing.

## Linux Distribution Specific Instructions

###  Ubuntu & Debian

#### Dependency Build Instructions

Build Requirements:

	sudo apt-get install build-essential libtool bsdmainutils autotools-dev autoconf pkg-config automake python3

Now, you can either build from self-compiled [depends](/depends/README.md) or install the required dependencies:

    sudo apt-get install libssl-dev libgmp-dev libevent-dev libboost-all-dev

**Note:** For Ubuntu versions starting with Bionic (18.04), or Debian versions starting with Stretch, use `libssl1.0-dev`
above instead of `libssl-dev`. WAGERR Core does not support the use of OpenSSL 1.1, though compilation is still possible
by passing `--with-incompatible-ssl` to configure (NOT RECOMMENDED!).

BerkeleyDB is required for the wallet.

 **For Ubuntu only:** db4.8 packages are available [here](https://launchpad.net/~bitcoin/+archive/bitcoin).
 You can add the repository using the following command:

        sudo apt-get install software-properties-common
        sudo add-apt-repository ppa:bitcoin/bitcoin
        sudo apt-get update
        sudo apt-get install libdb4.8-dev libdb4.8++-dev

Ubuntu and Debian have their own libdb-dev and libdb++-dev packages, but these will install
BerkeleyDB 5.1 or later. This will break binary wallet compatibility with the distributed executables, which
are based on BerkeleyDB 4.8. If you do not care about wallet compatibility,
pass `--with-incompatible-bdb` to configure.

To build Bitcoin Core without wallet, see [*Disable-wallet mode*](/doc/build-unix.md#disable-wallet-mode)

Optional (see --with-miniupnpc and --enable-upnp-default):

    sudo apt-get install libminiupnpc-dev

ZMQ dependencies (provides ZMQ API):

    sudo apt-get install libzmq3-dev

GUI dependencies:


If you want to build wagerr-qt, make sure that the required packages for Qt development
are installed. Qt 5 is necessary to build the GUI.
If both Qt 4 and Qt 5 are installed, Qt 5 will be used.
To build without GUI pass `--without-gui`.

For Qt 5 you need the following:

    sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler

libqrencode (optional) can be installed with:

    sudo apt-get install libqrencode-dev

Once these are installed, they will be found by configure and a wagerr-qt executable will be
built by default.

### Fedora

#### Dependency Build Instructions

Build requirements:

    sudo dnf install which gcc-c++ libtool make autoconf automake compat-openssl10-devel libevent-devel boost-devel libdb4-devel libdb4-cxx-devel gmp-devel python3

Optional:

    sudo dnf install miniupnpc-devel zeromq-devel

To build with Qt 5 you need the following:

    sudo dnf install qt5-qttools-devel qt5-qtbase-devel protobuf-devel

libqrencode (optional) can be installed with:

    sudo dnf install qrencode-devel


Notes
-----
The release is built with GCC and then "strip wagerrd" to strip the debug
symbols, which reduces the executable size by about 90%.


miniupnpc
---------

[miniupnpc](http://miniupnp.free.fr/) may be used for UPnP port mapping.  It can be downloaded from [here](
http://miniupnp.tuxfamily.org/files/).  UPnP support is compiled in and
turned off by default.  See the configure options for upnp behavior desired:

	--without-miniupnpc      No UPnP support miniupnp not required
	--disable-upnp-default   (the default) UPnP support turned off by default at runtime
	--enable-upnp-default    UPnP support turned on by default at runtime

To build:

	tar -xzvf miniupnpc-1.6.tar.gz
	cd miniupnpc-1.6
	make
	sudo su
	make install


Berkeley DB
-----------
It is recommended to use Berkeley DB 4.8. If you have to build it yourself:

```bash
WAGERR_ROOT=$(pwd)

# Pick some path to install BDB to, here we create a directory within the wagerr directory
BDB_PREFIX="${WAGERR_ROOT}/db4"
mkdir -p $BDB_PREFIX

# Fetch the source and verify that it is not tampered with
wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
echo '12edc0df75bf9abd7f82f821795bcee50f42cb2e5f76a6a281b85732798364ef  db-4.8.30.NC.tar.gz' | sha256sum -c
# -> db-4.8.30.NC.tar.gz: OK
tar -xzvf db-4.8.30.NC.tar.gz

# Build the library and install to our prefix
cd db-4.8.30.NC/build_unix/
#  Note: Do a static build so that it can be embedded into the exectuable, instead of having to find a .so at runtime
../dist/configure --enable-cxx --disable-shared --with-pic --prefix=$BDB_PREFIX
make install

# Configure WAGERR Core to use our own-built instance of BDB
cd $WAGERR_ROOT
./configure (other args...) LDFLAGS="-L${BDB_PREFIX}/lib/" CPPFLAGS="-I${BDB_PREFIX}/include/"
```

**Note**: You only need Berkeley DB if the wallet is enabled (see the section *Disable-Wallet mode* below).

Boost
-----
If you need to build Boost yourself:

	sudo su
	./bootstrap.sh
	./bjam install


Security
--------
To help make your WAGERR installation more secure by making certain attacks impossible to
exploit even if a vulnerability is found, binaries are hardened by default.
This can be disabled with:

Hardening Flags:

	./configure --enable-hardening
	./configure --disable-hardening

Hardening enables the following features:

* _Position Independent Executable_: Build position independent code to take advantage of Address Space Layout Randomization
    offered by some kernels. An attacker who is able to cause execution of code at an arbitrary
    memory location is thwarted if he doesn't know where anything useful is located.
    The stack and heap are randomly located by default but this allows the code section to be
    randomly located as well.

    On an Amd64 processor where a library was not compiled with -fPIC, this will cause an error
    such as: "relocation R_X86_64_32 against `......' can not be used when making a shared object;"

    To test that you have built PIE executable, install scanelf, part of paxutils, and use:

    	scanelf -e ./wagerrd

    The output should contain:
     TYPE
    ET_DYN

* _Non-executable Stack_: If the stack is executable then trivial stack based buffer overflow exploits are possible if
    vulnerable buffers are found. By default, wagerr should be built with a non-executable stack
    but if one of the libraries it uses asks for an executable stack or someone makes a mistake
    and uses a compiler extension which requires an executable stack, it will silently build an
    executable without the non-executable stack protection.

    To verify that the stack is non-executable after compiling use:
    `scanelf -e ./wagerrd`

    the output should contain:
	STK/REL/PTL
	RW- R-- RW-

    The STK RW- means that the stack is readable and writeable but not executable.

Disable-wallet mode
--------------------
**Note:** This functionality is not yet completely implemented, and compilation using the below option will currently fail.

When the intention is to run only a P2P node without a wallet, WAGERR Core may be compiled in
disable-wallet mode with:

    ./configure --disable-wallet

In this case there is no dependency on Berkeley DB 4.8.

Additional Configure Flags
--------------------------
A list of additional configure flags can be displayed with:

    ./configure --help