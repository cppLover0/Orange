
rm -rf pack
mkdir -p pack
mkdir -p cached

mkdir -p "$1/usr/include"
mkdir -p "$1/usr/bin"
mkdir -p "$1/usr/lib"
mkdir -p "$1/lib"
mkdir -p "$1/bin"

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

cd pack
 
export CFLAGS="-std=gnu11"

# mlibc doesn't need to be cached
git clone https://github.com/cpplover0/orange-mlibc --depth=1
cd orange-mlibc

sh build_to_cross.sh "$(realpath $1/..)"

T=$(realpath .)

cd $1/usr/lib
ln -sf ld.so ld64.so.1
cp -rf $HOME/opt/cross/orange/x86_64-orange-mlibc/lib/libs*.so* .

cd "$T"
cd ..
