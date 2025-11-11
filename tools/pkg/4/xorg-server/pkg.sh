
. ../../pkg-lib.sh
export PKG_CONFIG_PATh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

wget https://www.zlib.net/fossils/zlib-1.3.tar.gz

tar -xvf zlib-1.3.tar.gz

cd zlib-1.3

CHOST=x86_64-orange-mlibc prefix="/usr" ./configure
make -j$(nproc)
make install DESTDIR="$1"

cd ..

CFLAGS="-fPIC" fast_install "$1" "https://downloads.sourceforge.net/libpng/libpng-1.6.37.tar.gz"

fast_install "$1" https://downloads.sourceforge.net/freetype/freetype-2.12.1.tar.gz

wget https://www.x.org/pub/individual/util/util-macros-1.20.0.tar.gz
tar -xvf util-macros-1.20.0.tar.gz

cd util-macros-1.20.0

./configure --prefix="$HOME/opt/cross/orange"
make -j$(nproc)
make install 

cd ..

wget https://www.openssl.org/source/openssl-1.1.1q.tar.gz
tar -xvf openssl-1.1.1q.tar.gz

cd openssl-1.1.1q
diff_patch ../../diff/openssl.diff

CFLAGS="-Wno-implicit-function-declaration" CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-gcc ./Configure --prefix=/usr --openssldir=/etc/ssl --libdir=lib "x86_64-orange-mlibc" shared zlib-dynamic no-afalgeng

make -j$(nproc)

make install MANSUFFIX=ssl DESTDIR="$1"

cd ..

fast_install "$1" https://www.x.org/archive/individual/lib/xtrans-1.4.0.tar.gz

fast_install "$1" https://www.x.org/releases/individual/proto/xproto-7.0.31.tar.gz "--enable-shared" "../../diff/libxproto.diff"

fast_install "$1" https://www.x.org/releases/individual/proto/fontsproto-2.1.3.tar.gz

fast_install "$1" https://www.x.org/archive/individual/lib/libfontenc-1.1.8.tar.gz
fast_install "$1" https://www.x.org/archive/individual/lib/libXfont2-2.0.6.tar.gz
fast_install "$1" https://cairographics.org/releases/pixman-0.40.0.tar.gz
wget https://www.x.org/releases/individual/proto/xorgproto-2022.2.tar.gz

tar -xvf xorgproto-2022.2.tar.gz

cd xorgproto-2022.2

diff_patch ../../diff/xorgproto.diff
patch_config_sub "$(realpath $1/..)"

mkdir -p build0

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dlegacy=true build0

cd build0

ninja 
DESTDIR="$1" ninja install 

cd ../..

fast_install "$1" https://www.x.org/releases/individual/xcb/libpthread-stubs-0.5.tar.gz 

fast_install "$1" https://www.x.org/releases/individual/xcb/xcb-proto-1.15.tar.gz

fast_install "$1" https://www.x.org/releases/individual/lib/libXau-1.0.9.tar.gz 
fast_install "$1" https://www.x.org/releases/individual/xcb/libxcb-1.15.tar.gz 
fast_install "$1" https://www.x.org/releases/individual/lib/libX11-1.8.1.tar.gz 

fast_install "$1" https://www.x.org/archive/individual/lib/libxkbfile-1.1.1.tar.gz 

fast_install "$1" https://www.x.org/releases/individual/lib/libXfixes-6.0.0.tar.gz 

wget https://www.x.org/releases/individual/lib/libxcvt-0.1.2.tar.xz 
tar -xvf libxcvt-0.1.2.tar.xz

cd libxcvt-0.1.2

mkdir -p build

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix="/usr" build

cd build
ninja
DESTDIR="$1" ninja install
cd ../..

fast_install "$1" https://www.x.org/archive/individual/app/xkbcomp-1.4.5.tar.gz

wget http://www.x.org/releases/individual/data/xkeyboard-config/xkeyboard-config-2.36.tar.xz
tar -xvf xkeyboard-config-2.36.tar.xz

pushd xkeyboard-config-2.36
sed -i -E "s/(ln -s)/\1f/" rules/meson.build
popd

cd xkeyboard-config-2.36

mkdir -p build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix="/usr" build -Dxorg-rules-symlinks=true

cd build
ninja 
sudo DESTDIR="$1" ninja install

cd ../..

sudo chmod 777 -R "$1/usr/share/X11/xkb"

fast_install "$1" https://www.x.org/releases/individual/xserver/xorg-server-21.1.4.tar.gz "--with-xkb-bin-directory=/usr/bin --disable-pciaccess --disable-libdrm --disable-glx --disable-int10-module --disable-glamor --disable-vgahw --disable-dri3 --disable-dri2 --disable-dri --disable-xephyr --disable-xwayland --disable-xnest --disable-dmx --with-fontrootdir=/usr/share/fonts/X11 --disable-strict-compilation" "../../diff/xorgserver.diff"


# at this point i just tired so ill use ironclad patches

mkdir -p "$1"/usr/lib/xorg/modules/extensions