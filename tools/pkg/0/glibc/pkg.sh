. ../../pkg-lib.sh

if [ ! -d "./linux-headers" ]; then
    mkdir -p temp
    mkdir -p linux-headers
    git clone https://github.com/torvalds/linux.git --depth=1
    cd linux
    make headers_install ARCH=x86_64 INSTALL_HDR_PATH="$(realpath ../temp)"
    cd ..
    cp -rf temp/include/* linux-headers
    rm -rf temp
    rm -rf linux
fi

echo Copying linux-headers...
mkdir -p "$1/usr/include"
cp -rf linux-headers/* "$1/usr/include"

rm -rf pack
mkdir -p pack
cd pack

mkdir -p "$1"

wget $GNU_MIRROR/gnu/libc/glibc-2.42.tar.gz
tar -xvf glibc-2.42.tar.gz
cd glibc-2.42

T="$(pwd)"

cd "$1"
rm -rf lib bin 
echo "Creating symlinks"

ln -sf usr/lib lib
ln -sf usr/bin bin

cd usr

cd "$T"

mkdir -p glibc-build
cd glibc-build
../configure --prefix="/usr" --disable-debug --disable-multi-arch --disable-multilib --disable-nls --disable-profile --libdir=/usr/lib --disable-static --enable-shared --with-sysroot="$1" lt_cv_sys_lib_dlsearch_path_spec="/usr/lib"
make -j$(nproc)
make DESTDIR="$1" install -j$(nproc)
cd "$1/usr/lib"
#cp -rf "$1/lib64/*" "$1/usr/lib"
cp -rf "$HOME"/opt/cross/orange/x86_64-linux-gnu/lib/libs*.so* .
cp -rf "$HOME"/opt/cross/orange/lib64/libg*.so* .
cp -rf "$HOME"/opt/cross/orange/lib64/libs*.so* .
cd "$1/usr"
#ln -sf lib lib64
cd "$1"
#rm -rf "lib64"
#ln -sf usr/lib lib64

cp -rf lib64/* "$1/usr/lib"
rm -rf lib64
ln -sf usr/lib lib64

cd "$T"

cp -rf "/usr/include/sys/sdt.h" "/usr/include/sys/sdt-config.h" "$1/usr/include/sys" # copy it if available 

rm -rf "$1/usr/lib/libc.a" "$1/usr/lib/libm-2.42.a" "$1/usr/lib/libmvec.a"
echo "glibc compile done"

cd ..