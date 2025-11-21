
# 


. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

git clone https://github.com/lloyd/yajl.git --depth=1

cd yajl

mkdir -p "$1/usr/include/yajl/api"

cp -rf src/*.h "$1/usr/include/yajl"
cp -rf src/api/*.h "$1/usr/include/yajl/"

cp -rf ../../yajl_version.h "$1/usr/include/yajl/yajl_version.h"

x86_64-orange-mlibc-gcc -c src/*.c -Isrc

x86_64-orange-mlibc-ar rcs "$1/usr/lib/libyajl.a" ./*.o

cp -rf ../../yajl.pc "$1/usr/lib/pkgconfig"

cd ..

git clone https://github.com/Airblader/xcb-util-xrm.git --depth=1
cd xcb-util-xrm

cp -rf include/xcb_xrm.h "$1/usr/include/xcb"
x86_64-orange-mlibc-gcc -c src/database.c src/entry.c src/match.c src/resource.c src/util.c -Iinclude -Wno-implicit-function-declaration

x86_64-orange-mlibc-ar rcs "$1/usr/lib/libxcb-xrm.a" database.o entry.o match.o resource.o util.o

cp -rf ../../xcb-xrm.pc.in "$1/usr/lib/pkgconfig/xcb-xrm.pc"

cd ..

wget https://github.com/xkbcommon/libxkbcommon/archive/refs/tags/xkbcommon-1.13.0.tar.gz
tar -xvf xkbcommon-1.13.0.tar.gz

cd libxkbcommon-xkbcommon-1.13.0

mkdir -p build

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Denable-wayland=false -Denable-x11=true build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

fast_install "$1" http://www.freedesktop.org/software/startup-notification/releases/startup-notification-0.12.tar.gz "lf_cv_sane_realloc=1"

git clone https://github.com/enki/libev.git --depth=1
cd libev

autotools_recursive_regen
./configure --host=x86_64-orange-mlibc --prefix=/usr
make -j$(nproc)
DESTDIR="$1" make install 

cd ..

wget https://i3wm.org/downloads/i3-4.24.tar.xz
tar -xvf i3-4.24.tar.xz

cd i3-4.24

mkdir -p build

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

cd ..