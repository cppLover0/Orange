
# https://archive.xfce.org/src/xfce/xfwm4/4.19/xfwm4-4.19.0.tar.bz2


. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack
cd pack

# wget https://archive.fr.xfce.org/src/xfce/libxfce4ui/4.21/libxfce4ui-4.21.3.tar.xz
# tar -xvf libxfce4ui-4.21.3.tar.xz
# cd libxfce4ui-4.21.3

# mkdir build
# meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dintrospection=false build

# cd build

# meson compile -j$(nproc)
# DESTDIR="$1" meson install --no-rebuild

# cd ../..

git clone https://gitlab.gnome.org/GNOME/dconf.git --depth=1 --recursive
cd dconf

sed -i 's/install_dir: systemd_userunitdir,//' service/meson.build

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dvapi=false build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild
cd ../..


fast_install "$1" https://downloads.xiph.org/releases/ogg/libogg-1.3.6.tar.xz
fast_install "$1" https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz
fast_install "$1" https://0pointer.de/lennart/projects/libcanberra/libcanberra-0.30.tar.xz "--disable-oss --disable-gtk --disable-gstreamer --disable-pulse --disable-alsa"
fast_install "$1" https://salsa.debian.org/iso-codes-team/iso-codes/-/archive/v4.19.0/iso-codes-v4.19.0.tar.gz
fast_install "$1" https://www.x.org/releases/individual/lib/libXres-1.2.3.tar.gz
fast_install "$1" https://github.com/mate-desktop/mate-desktop/releases/download/v1.28.2/mate-desktop-1.28.2.tar.xz "--disable-introspection"
export CFLAGS="-lXRes -O2"

wget https://github.com/GNOME/libwnck/archive/refs/tags/43.0.tar.gz
tar -xvf 43.0.tar.gz
cd libwnck-43.0

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dintrospection=disabled build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

git clone https://gitlab.gnome.org/GNOME/libnotify.git
cd libnotify
mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dintrospection=disabled -Dtests=false -Dgtk_doc=false build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

fast_install "$1" https://github.com/mate-desktop/marco/releases/download/v1.29.1/marco-1.29.1.tar.xz "--disable-compositor --disable-introspection" "../../diff/marco.diff"
fast_install "$1" https://github.com/mate-desktop/mate-themes/releases/download/v3.22.26/mate-themes-3.22.26.tar.xz "--enable-shared" "../../diff/mate-themes.diff"

fast_install "$1" https://github.com/mate-desktop/libmateweather/releases/download/v1.28.0/libmateweather-1.28.0.tar.xz "--enable-locations-compression  --disable-all-translations-in-one-xml --disable-icon-update"
fast_install "$1" https://github.com/mate-desktop/mate-menus/releases/download/v1.28.1/mate-menus-1.28.1.tar.xz "--disable-introspection"
fast_install "$1" https://github.com/mate-desktop/mate-panel/releases/download/v1.28.7/mate-panel-1.28.7.tar.xz "--disable-introspection"
fast_install "$1" https://github.com/mate-desktop/caja/releases/download/v1.28.0/caja-1.28.0.tar.xz "--disable-update-mimedb --disable-xmp --without-libnotify --enable-introspection=no"
glib-compile-schemas "$1/usr"/share/glib-2.0/schemas

fast_install "$1" https://people.freedesktop.org/~svu/libxklavier-5.4.tar.bz2 "--disable-gtk-doc --disable-introspection --disable-vala --with-xkb-base=/usr/share/X11/xkb --with-xkb-bin-base=/usr/bin"
fast_install "$1" https://github.com/mate-desktop/libmatekbd/releases/download/v1.28.0/libmatekbd-1.28.0.tar.xz "--with-x --disable-introspection --without-tests"
fast_install "$1" https://github.com/mate-desktop/mate-settings-daemon/releases/download/v1.28.0/mate-settings-daemon-1.28.0.tar.xz " --with-x --without-libnotify  --with-libcanberra --without-libmatemixer --disable-debug --disable-polkit --disable-pulse --disable-rfkill --disable-smartcard-support"

cd ..