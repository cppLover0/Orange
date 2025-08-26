. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

installgnu bash bash 5.2.21
mkdir -p bash-build

cd bash-5.2.21
diff_patch ../../diff/bash.diff
cd ..

cd bash-build
../bash-5.2.21/configure --host=x86_64-orange --prefix="/usr" --disable-readline --with-curses --without-bash-malloc CFLAGS="-std=gnu17 -fPIC -Os"
make install-strip -j$(nproc) DESTDIR="$1"

cz=$(pwd)

cd "$1/usr/bin"
ln -sf bash sh

cd "$cz"

cd ..