
. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack
cd pack

fast_install "$1" https://github.com/rockdaboot/libpsl/releases/download/0.21.5/libpsl-0.21.5.tar.gz

wget https://mirrors.dotsrc.org/gnome/sources/libsoup/2.74/libsoup-2.74.0.tar.xz
tar -xvf libsoup-2.74.0.tar.xz
cd libsoup-2.74.0
mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dvapi=disabled -Dintrospection=disabled -Dgssapi=disabled -Dsysprof=disabled -Dtls_check=false build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

cd ..