. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

fast_install "$1" $GNU_MIRROR/gnu/libiconv/libiconv-1.18.tar.gz
fast_install "$1" $GNU_MIRROR/gnu/gettext/gettext-0.26.tar.gz "--enable-shared" ../../diff/gettext.diff

installgnu bash bash 5.2.21
mkdir -p bash-build

cd bash-5.2.21
patch_config_sub "$(realpath $1/..)"
cd ..

cd bash-build
../bash-5.2.21/configure --host=x86_64-orange-mlibc --prefix="/usr" --disable-readline --with-curses --without-bash-malloc CFLAGS="-std=gnu17 -fPIC -Os"
make install-strip -j$(nproc) DESTDIR="$1"

cz=$(pwd)

cd "$1/usr/bin"
ln -sf bash sh

cd "$cz"

cd ..
