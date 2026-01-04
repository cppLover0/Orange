
. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

# git clone https://github.com/libexpat/libexpat.git
# cd libexpat
# mkdir build
# cd build

# cmake ../expat -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../../../../toolchain.cmake) -DCMAKE_INSTALL_PREFIX="/usr"

# cmake --build . -j$(nproc)
# DESTDIR="$1" cmake --install .

# cd ../..

fast_install "$1" https://www.x.org/archive/individual/lib/libXtst-1.2.5.tar.gz

git clone https://gitlab.gnome.org/GNOME/libxml2.git --depth=1
cd libxml2

mkdir build

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dpython=disabled -Dhistory=disabled -Dicu=disabled -Dreadline=disabled -Dzlib=enabled build
cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://gitlab.freedesktop.org/dbus/dbus/-/archive/dbus-1.16.2/dbus-dbus-1.16.2.tar.gz
tar -xvf dbus-dbus-1.16.2.tar.gz
cd dbus-dbus-1.16.2

diff_patch ../../diff/dbus.diff

mkdir build

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Druntime_dir=/run -Dsystemd_system_unitdir=no -Dsystemd_user_unitdir=no -Dsystem_pid_file=/run/dbus/pid -Dsystem_socket=/run/dbus/system_bus_socket -Dselinux=disabled -Dapparmor=disabled -Dlibaudit=disabled -Dkqueue=disabled -Dlaunchd=disabled -Dsystemd=disabled -Dmodular_tests=disabled -Depoll=disabled -Dverbose_mode=true build
cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

touch "$1/usr/share/dbus-1/session.d/.keep"
touch "$1/var/lib/dbus/.keep"

cd ../..

exit 0

wget https://gitlab.gnome.org/GNOME/at-spi2-core/-/archive/2.56.6/at-spi2-core-2.56.6.tar
tar -xvf at-spi2-core-2.56.6.tar
cd at-spi2-core-2.56.6

mkdir build 

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" -Dsystemd_user_dir=/tmp -Dintrospection=disabled --prefix=/usr build
cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://github.com/GNOME/gobject-introspection/archive/refs/tags/1.86.0.tar.gz
mv 1.86.0.tar.gz intro.tar.gz
tar -xvf intro.tar.gz

cd gobject-introspection-1.86.0

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dgtk_doc=false -Dbuild_introspection_data=false build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

cd ..