
. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

fast_install "$1" https://github.com/libffi/libffi/releases/download/v3.5.2/libffi-3.5.2.tar.gz
fast_install "$1" $GNU_MIRROR/gnu/libiconv/libiconv-1.18.tar.gz

git clone https://github.com/PCRE2Project/pcre2.git --depth=1
cd pcre2

autotools_recursive_regen
cd ..

mkdir -p build2
cd build2
cmake ../pcre2 -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../../../toolchain.cmake) -DCMAKE_INSTALL_PREFIX="/usr"

cmake --build . -j$(nproc)
DESTDIR="$1" cmake --install .

cd ..

wget https://github.com/GNOME/glib/archive/refs/tags/2.86.1.tar.gz
mv 2.86.1.tar.gz glib2.tar.gz
tar -xvf glib2.tar.gz

cd glib-2.86.1

cd subprojects
rm -rf gvdb 
cd ..

diff_patch ../../diff/glib.diff
autotools_recursive_regen

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dglib_debug=disabled -Dsysprof=disabled -Dintrospection=disabled -Dxattr=false build

cd build
meson compile -j$(nproc)

DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://cairographics.org/releases/cairo-1.18.4.tar.xz
tar -xvf cairo-1.18.4.tar.xz
cd cairo-1.18.4

mkdir -p build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dxlib-xcb=enabled -Dzlib=enabled -Dtee=enabled -Dtests=disabled build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://github.com/harfbuzz/harfbuzz/archive/refs/tags/5.0.0.tar.gz
mv 5.0.0.tar.gz harfbuzz-5.0.0.tar.gz
tar -xvf harfbuzz-5.0.0.tar.gz
cd harfbuzz-5.0.0

mkdir -p build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dgraphite2=disabled -Dglib=enabled -Dgobject=disabled -Dicu=disabled -Dfreetype=enabled -Dcairo=enabled -Dintrospection=disabled -Dtests=disabled build

cd build
meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://github.com/GNOME/pango/archive/refs/tags/1.49.2.tar.gz
mv 1.49.2.tar.gz pango-1.49.2.tar.gz
tar -xvf pango-1.49.2.tar.gz
cd pango-1.49.2

mkdir -p build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dintrospection=disabled build

cd build
meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://github.com/GNOME/gdk-pixbuf/archive/refs/tags/2.42.10.tar.gz
mv 2.42.10.tar.gz gdk-pixbuf-2.42.10.tar.gz
tar -xvf gdk-pixbuf-2.42.10.tar.gz
cd gdk-pixbuf-2.42.10

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dgio_sniffing=false -Dman=false build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

# patch_config_sub "$(realpath $1/..)"

# mkdir -p build0

# 

# cd build0

cd ..