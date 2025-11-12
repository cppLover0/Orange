
. ../../pkg-lib.sh

rm -rf pack
mkdir pack

cd pack

fast_install "$1" https://www.x.org/releases/individual/xcb/xcb-util-0.4.1.tar.gz
fast_install "$1" https://www.x.org/releases/individual/xcb/xcb-util-image-0.4.1.tar.gz

git clone https://github.com/stoeckmann/xwallpaper.git

cd xwallpaper

./autogen.sh
autotools_recursive_regen
./configure --host=x86_64-orange-mlibc --prefix="/usr"
make -j$(nproc)
make install DESTDIR="$1"

cd ..