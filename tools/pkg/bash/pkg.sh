
. ../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

install_gnu bash bash 5.2.21
mkdir -p bash-build

cd bash-5.2.21
diff_patch ../../diff/bash.diff
cd ..

cd bash-build
../bash-5.2.21/configure --host=x86_64-orange --prefix=$1 --disable-readline --without-bash-malloc CFLAGS="-std=gnu17 -fPIC"
make install -j$(nproc)

cz=$(pwd)

cd $1/bin
ln -s bash sh

cd "$cz"

clear_share "$1"

cd ..