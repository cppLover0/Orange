
. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

wget https://github.com/GNOME/gobject-introspection/archive/refs/tags/1.73.0.tar.gz
mv 1.73.0.tar.gz intro.tar.gz
tar -xvf intro.tar.gz

cd gobject-introspection-1.73.0

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dgtk_doc=false -Dbuild_introspection_data=false build

cd build

exit 0

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

wget https://codeload.github.com/GNOME/gtk/tar.gz/refs/tags/3.24.22
mv 3.24.22 gtk-3.24.22.tar.gz

tar -xvf gtk-3.24.22.tar.gz

cd gtk-3.24.22

autotools_recursive_regen
mkdir -p build

meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dprint_backends=file  -Dintrospection=false -Dx11_backend=true -Dbroadway_backend=true -Dwayland_backend=false -Dcolord=no build

cd build
meson compile -j$(nproc)

sudo chmod -R 777 "$1/usr/share/locale" 

DESTDIR="$1" meson install --no-rebuild

glib-compile-schemas "$1/usr"/share/glib-2.0/schemas
rm "$1/usr"/share/glib-2.0/schemas/gschemas.compiled

cd ..

# patch_config_sub "$(realpath $1/..)"

# mkdir -p build0

# 

# cd build0

cd ..