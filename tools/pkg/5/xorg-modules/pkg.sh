
. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack

cd pack

ca="$(pwd)"

cd "$1/usr/lib"

for file in xorg/modules/*.so; do ln -s "$file" .; done

cd "$ca"

fast_install "$1" https://www.x.org/archive//individual/font/font-util-1.4.1.tar.xz

fast_install "$1" https://www.x.org/pub/individual/font/font-alias-1.0.5.tar.xz

CFLAGS="-fPIC" SYSROOT="$1/" fast_install "$1" https://www.x.org/releases/individual/driver/xf86-video-fbdev-0.5.1.tar.gz "--disable-pciaccess --disable-static --enable-shared" "../../diff/xfbdev.diff"

git clone https://gitlab.freedesktop.org/libevdev/libevdev.git
cd libevdev

mkdir build
meson -Ddocumentation=disabled -Dtests=disabled --prefix=/usr build
cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

git clone https://github.com/illiliti/libudev-zero.git
cd libudev-zero

export CC=x86_64-linux-gnu-gcc
export AR=x86_64-linux-gnu-ar

make -j$(nproc)
DESTDIR="$1" make install

cd ..

git clone https://gitlab.gnome.org/GNOME/libgudev.git --depth=1
cd libgudev

mkdir build
meson --prefix=/usr build
cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

fast_install "$1" https://bitmath.org/code/mtdev/mtdev-1.1.7.tar.gz

git clone https://github.com/linuxwacom/libwacom.git --depth=1
cd libwacom

mkdir build
meson --prefix=/usr build
cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

git clone https://gitlab.freedesktop.org/libinput/libinput.git --depth=1
cd libinput

mkdir build
meson  -Dtests=false --prefix=/usr build
cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..


CFLAGS="-fPIC"  SYSROOT="$1/" fast_install "$1" https://www.x.org/releases/individual/driver/xf86-input-libinput-1.5.0.tar.gz 
CFLAGS="-fPIC" SYSROOT="$1/" fast_install "$1" https://www.x.org/releases/individual/driver/xf86-input-evdev-2.11.0.tar.gz "--disable-static --enable-shared --disable-udev --without-libudev" 