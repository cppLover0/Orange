
. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

wget https://github.com/anholt/libepoxy/archive/refs/tags/1.5.10.tar.gz
tar -xvf 1.5.10.tar.gz
cd libepoxy-1.5.10

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Degl=no -Dtests=false build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..


wget https://codeload.github.com/GNOME/gtk/tar.gz/refs/tags/3.24.51
mv 3.24.51 gtk-3.24.51.tar.gz

tar -xvf gtk-3.24.51.tar.gz

cd gtk-3.24.51

autotools_recursive_regen
mkdir -p build

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dprint_backends=file  -Dintrospection=false -Dx11_backend=true -Dbroadway_backend=true -Dwayland_backend=false -Dcolord=no build

cd build
meson compile -j$(nproc)

sudo chmod -R 777 "$1/usr/share/locale" 

DESTDIR="$1" meson install --no-rebuild

glib-compile-schemas "$1/usr"/share/glib-2.0/schemas

cd ../../..