# https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz


. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

fast_install "$1" https://ftp.gnu.org/gnu/libtool/libtool-2.5.4.tar.xz

# wget https://ftpmirror.gnu.org/gnu/libtool/libtool-2.5.4.tar.gz
# tar -xvf libtool-2.5.4.tar.gz
# cd libtool-2.5.4

# diff_patch ../../diff/libtool.diff
# ./configure --prefix="$HOME/opt/cross/orange" --with-gnu-ld --enable-shared --disable-static
# make -j$(nproc)
# make install

# cd ..

cd ..
