
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

wget https://ftp.debian.org/debian/pool/main/x/xfonts-base/xfonts-base_1.0.5_all.deb
ar -x xfonts-base_1.0.5_all.deb

tar -xvf data.tar.xz
cp -rf usr/* "$1/usr"
cp -rf etc/* "$1/etc"

CFLAGS="-fPIC" SYSROOT="$1/" fast_install "$1" https://www.x.org/releases/individual/driver/xf86-video-fbdev-0.5.1.tar.gz "--disable-pciaccess --disable-static --enable-shared" "../../diff/xfbdev.diff"
CFLAGS="-fPIC" SYSROOT="$1/" fast_install "$1" https://www.x.org/releases/individual/driver/xf86-input-keyboard-2.1.0.tar.gz "--disable-static --enable-shared" "../../diff/xkeyboard.diff"
