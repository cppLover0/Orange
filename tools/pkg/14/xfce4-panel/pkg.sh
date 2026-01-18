
# https://archive.xfce.org/src/xfce/xfwm4/4.19/xfwm4-4.19.0.tar.bz2


. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack
cd pack

wget https://archive.fr.xfce.org/src/xfce/libxfce4ui/4.21/libxfce4ui-4.21.3.tar.xz
tar -xvf libxfce4ui-4.21.3.tar.xz
cd libxfce4ui-4.21.3

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dintrospection=false build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://archive.xfce.org/src/xfce/garcon/4.21/garcon-4.21.0.tar.xz
tar -xvf garcon-4.21.0.tar.xz
cd garcon-4.21.0

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dintrospection=false build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://gitlab.freedesktop.org/emersion/libdisplay-info/-/archive/0.3.0/libdisplay-info-0.3.0.tar.bz2
tar -xvf libdisplay-info-0.3.0.tar.bz2
cd libdisplay-info-0.3.0

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

fast_install "$1" https://archive.xfce.org/src/xfce/libxfce4windowing/4.20/libxfce4windowing-4.20.5.tar.bz2 "--disable-introspection"

wget https://archive.xfce.org/src/xfce/xfce4-panel/4.21/xfce4-panel-4.21.1.tar.xz
tar -xvf xfce4-panel-4.21.1.tar.xz
cd xfce4-panel-4.21.1

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dintrospection=false build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

#fast_install "$1" https://archive.xfce.org/src/art/xfwm4-themes/4.10/xfwm4-themes-4.10.0.tar.bz2 "--sysconfdir=/etc"

cd ..