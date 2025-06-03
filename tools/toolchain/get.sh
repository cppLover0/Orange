
set -e

GNU_MIRROR=https://mirror.dogado.de/
CURRENT_DIR="$(realpath .)"

rm -rf "$HOME/opt/cross/orange"

rm -rf pack

echo Downloading binutils and gcc

mkdir -p pack

mkdir -p $1/initrd/usr/include
mkdir -p $1/initrd/usr/lib

cd pack

wget -nc $GNU_MIRROR/gnu/gcc/gcc-11.4.0/gcc-11.4.0.tar.gz
wget -nc $GNU_MIRROR/gnu/binutils/binutils-2.38.tar.gz

echo Unpacking binutils and gcc

tar -xvf gcc-11.4.0.tar.gz
tar -xvf binutils-2.38.tar.gz

echo Patching binutils and gcc

cd binutils-2.38
patch -p1 <"$CURRENT_DIR/../diffs/binutils.diff"
cd ../gcc-11.4.0
patch -p1 <"$CURRENT_DIR/../diffs/gcc.diff"
cd ..

echo Downloading prerequisites
cd gcc-11.4.0
./contrib/download_prerequisites
cd ..

echo Donwloading and installing automake and autoconf 

wget -nc $GNU_MIRROR/gnu/automake/automake-1.15.1.tar.gz
wget -nc $GNU_MIRROR/gnu/autoconf/autoconf-2.69.tar.gz

tar -xvf automake-1.15.1.tar.gz
tar -xvf autoconf-2.69.tar.gz

cd automake-1.15.1
./configure --prefix="$HOME/opt/cross/orange"
make
make install 
cd ..

cd autoconf-2.69
./configure --prefix="$HOME/opt/cross/orange"
make
make install 
cd ..

export PATH="$HOME/opt/cross/orange/bin:$PATH"

cd binutils-2.38/ld
automake 
cd ../../gcc-11.4.0/libstdc++-v3
autoconf
cd ../../

echo Building binutils and gcc

mkdir -p binutils-build
cd binutils-build
mkdir -p $1/initrd
../binutils-2.38/configure --target=x86_64-orange --prefix="$HOME/opt/cross/orange" --with-sysroot="$(realpath $1)/initrd" --enable-shared
make -j$(nproc)
make install -j$(nproc)

cd ..
mkdir -p gcc-build
cd gcc-build
../gcc-11.4.0/configure --target=x86_64-orange --prefix="$HOME/opt/cross/orange" --with-sysroot="$(realpath $1)/initrd" --enable-languages=c,c++,go --disable-nls --enable-linker-build-id --enable-default-pie --enable-default-ssp --disable-multilib --enable-initfini-array --enable-shared --enable-host-shared CFLAGS=""

make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
make install-gcc -j$(nproc)
make install-target-libgcc -j$(nproc)
make all-target-libstdc++-v3
make install-target-libstdc++-v3

echo Done
