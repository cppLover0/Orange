
. ../../pkg-lib.sh

rm -rf pack
mkdir pack

cd pack

fast_install "$1" https://www.x.org/releases/individual/xcb/xcb-util-0.4.1.tar.gz
fast_install "$1" https://www.x.org/releases/individual/xcb/xcb-util-image-0.4.1.tar.gz

git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git --depth=1
cd libjpeg-turbo

autotools_recursive_regen

cd ..

mkdir -p libjpeg-build
cd libjpeg-build


CFLAGS="-fPIC -Wno-implicit-function-declaration -O2" cmake ../libjpeg-turbo -DBUILD_SHARED_LIBS=OFF -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../../../toolchain.cmake) -DCMAKE_INSTALL_PREFIX="$1/usr" 

make -j$(nproc)
make install 

rm -rf "$1/usr/lib/libjpeg.so"

cd ..

git clone https://github.com/stoeckmann/xwallpaper.git

cd xwallpaper

./autogen.sh
autotools_recursive_regen
./configure --host=x86_64-orange-mlibc --prefix="/usr"
make -j$(nproc)
make install DESTDIR="$1"

cd ..
