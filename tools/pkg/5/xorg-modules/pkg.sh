
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
CFLAGS="-fPIC" SYSROOT="$1/" fast_install "$1" https://www.x.org/releases/individual/driver/xf86-input-keyboard-2.1.0.tar.gz "--disable-static --enable-shared" "../../diff/xkeyboard.diff"
CFLAGS="-fPIC" SYSROOT="$1/" fast_install "$1" https://xorg.freedesktop.org/archive/individual/driver/xf86-input-mouse-1.9.5.tar.gz "--enable-shared" "../../diff/xmouse.diff"