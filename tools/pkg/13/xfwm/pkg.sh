
# https://archive.xfce.org/src/xfce/xfwm4/4.19/xfwm4-4.19.0.tar.bz2


. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack
cd pack

fast_install "$1" https://archive.xfce.org/src/xfce/libxfce4util/4.19/libxfce4util-4.19.0.tar.bz2 "--disable-shared --enable-static --disable-introspection"
fast_install "$1" https://archive.xfce.org/src/xfce/xfconf/4.19/xfconf-4.19.0.tar.bz2 "--disable-introspection"
fast_install "$1" https://archive.xfce.org/src/xfce/libxfce4ui/4.19/libxfce4ui-4.19.0.tar.bz2 "--disable-introspection"

# why not to compile libwnck-4
git clone https://gitlab.gnome.org/GNOME/libwnck.git --depth=1
cd libwnck

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dintrospection=disabled build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

# now compile libwnck-3
wget https://github.com/GNOME/libwnck/archive/refs/tags/3.36.0.tar.gz
tar -xvf 3.36.0.tar.gz
cd libwnck-3.36.0

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dintrospection=disabled build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

fast_install "$1" https://archive.xfce.org/src/xfce/xfwm4/4.19/xfwm4-4.19.0.tar.bz2 "--disable-introspection"

wget https://icon-theme.freedesktop.org/releases/hicolor-icon-theme-0.18.tar.xz
tar -xvf hicolor-icon-theme-0.18.tar.xz
cd hicolor-icon-theme-0.18

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://gitlab.freedesktop.org/xdg/shared-mime-info/-/archive/2.4/shared-mime-info-2.4.tar.gz
tar -xvf shared-mime-info-2.4.tar.gz
cd shared-mime-info-2.4

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

cd ..