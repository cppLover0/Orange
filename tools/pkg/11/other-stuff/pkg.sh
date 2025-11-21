
. ../../pkg-lib.sh

mkdir -p "$1/usr/share/gtk-3.0/"
mkdir -p "$1/etc/gtk-3.0"
cp -rf settings.ini "$1/usr/share/gtk-3.0/settings.ini"
cp -rf settings.ini "$1/etc/gtk-3.0/settings.ini"

rm -rf pack
mkdir -p pack

cd pack

fast_install "$1" https://www.x.org/releases/individual/xcb/xcb-util-renderutil-0.3.10.tar.gz
fast_install "$1" https://www.x.org/releases/individual/xcb/xcb-util-cursor-0.1.6.tar.gz
fast_install "$1" https://www.x.org/releases/individual/xcb/xcb-util-keysyms-0.4.1.tar.gz
fast_install "$1" https://www.x.org/releases/individual/xcb/xcb-util-wm-0.4.2.tar.gz



fast_install "$1" https://www.x.org/releases/individual/app/xrandr-1.5.2.tar.gz
fast_install "$1" https://www.x.org/releases/individual/app/xdpyinfo-1.3.4.tar.gz

wget https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-fonts-ttf-2.37.tar.bz2
tar -xvf dejavu-fonts-ttf-2.37.tar.bz2

cd dejavu-fonts-ttf-2.37
mkdir -p "$1/usr/share/fonts/truetype/dejavu"
cp -rf ttf/* "$1/usr/share/fonts/truetype/dejavu"

cd ..