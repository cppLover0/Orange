
. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack
cd pack

fast_install "$1" https://github.com/nghttp2/nghttp2/releases/download/v1.68.0/nghttp2-1.68.0.tar.gz

wget https://github.com/sqlite/sqlite/archive/refs/tags/version-3.51.1.tar.gz
tar -xvf version-3.51.1.tar.gz
cd sqlite-version-3.51.1
autotools_recursive_regen
./configure --prefix=/usr --host=x86_64-orange-mlibc --sysconfdir=/etc --localstatedir=/var --bindir=/usr/bin --sbindir=/usr/bin --libdir=/usr/lib --soname=legacy --disable-static --enable-shared --enable-fts4 --enable-fts5 --disable-tcl
make -j$(nproc)
make install DESTDIR="$1"
cd ..

cd ..