
. pkg/pkg-lib.sh

export CFLAGS="-fPIC"
export CXXFLAGS="-fPIC"
export CPPFLAGS="-fPIC"

GNU_MIRROR=https://mirror.dogado.de/
CURRENT_DIR="$(realpath .)"

 rm -rf "$HOME/opt/cross/orange"

 rm -rf pack

 echo Downloading binutils and gcc

 mkdir -p pack

 mkdir -p $1/initrd/usr/include
 mkdir -p $1/initrd/usr/lib

 cd pack

 wget -nc $GNU_MIRROR/gnu/gcc/gcc-15.1.0/gcc-15.1.0.tar.gz
 wget -nc $GNU_MIRROR/gnu/binutils/binutils-2.38.tar.gz

 echo Unpacking binutils and gcc

 tar -xvf gcc-15.1.0.tar.gz
 tar -xvf binutils-2.38.tar.gz

 echo Patching binutils and gcc

 echo Downloading prerequisites

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

 cd gcc-15.1.0
 cp -rf "$1/tools/pkg/config.sub" "$1/tools/pkg/config.guess" . 
 cd ..

 cd binutils-2.38
 cp -rf "$1/tools/pkg/config.sub" "$1/tools/pkg/config.guess" . 
 cd ..

 echo Building binutils and gcc

 mkdir -p binutils-build
 cd binutils-build
 mkdir -p $1/initrd
 ../binutils-2.38/configure --target=x86_64-linux-gnu --prefix="$HOME/opt/cross/orange" --with-sysroot="$(realpath $1)/initrd" --enable-shared
 make -j$(nproc)
 make install -j$(nproc)

 export CFLAGS="-fPIC"
 export CXXFLAGS="-fPIC"

 cd ..

rm -rf gcc-build
mkdir -p gcc-build
cd gcc-build

export CPP=/bin/cpp
export CXX=/bin/g++

echo hi

cp -rf "/usr/include/sys/sdt.h" "/usr/include/sys/sdt-config.h" "$1/initrd/usr/include/sys" # copy it if available 

../gcc-15.1.0/configure --without-dtrace --disable-systemtap --target=x86_64-linux-gnu --prefix="$HOME/opt/cross/orange" --with-sysroot="$(realpath $1)/initrd" --enable-languages=c,c++,go --disable-nls --with-pic --enable-linker-build-id --enable-threads=posix --enable-default-pie --enable-default-ssp --disable-multilib --enable-initfini-array --enable-shared --enable-host-shared  --with-mpc --with-mpfr --with-gmp --with-build-config=bootstrap-lto --with-linker-hash-style=gnu --with-system-zlib --enable-__cxa_atexit --enable-cet=auto --enable-checking=release --enable-clocale=gnu --enable-default-pie --enable-default-ssp --enable-gnu-indirect-function --enable-gnu-unique-object --enable-libstdcxx-backtrace --enable-link-serialization=1 --enable-linker-build-id --enable-lto --disable-multilib --enable-plugin --enable-shared --enable-threads=posix --disable-libssp --disable-libstdcxx-pch --disable-werror

echo no

make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
make install-gcc -j$(nproc)
make install-target-libgcc -j$(nproc)

make all-target-libstdc++-v3 -j$(nproc)
make install-target-libstdc++-v3 

T="$(pwd)"
cd "$1/initrd/usr/lib"
ln -sf ld.so ld64.so.1
cp -rf "$HOME"/opt/cross/orange/x86_64-linux-gnu/lib/libs*.so* .
cp -rf "$HOME"/opt/cross/orange/lib64/libg*.so* .
cp -rf "$HOME"/opt/cross/orange/lib64/libs*.so* .
cd "$T"

echo Done
