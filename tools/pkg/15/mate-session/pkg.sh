#


# https://archive.xfce.org/src/xfce/xfwm4/4.19/xfwm4-4.19.0.tar.bz2


. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack
cd pack

fast_install "$1" https://www.x.org/archive/individual/lib/libXcomposite-0.4.6.tar.gz
fast_install "$1" https://dbus.freedesktop.org/releases/dbus-glib/dbus-glib-0.114.tar.gz "--disable-gtk-doc --with-dbus-binding-tool=dbus-binding-tool"
fast_install "$1" https://github.com/mate-desktop/mate-session-manager/releases/download/v1.28.0/mate-session-manager-1.28.0.tar.xz

cd ..