
. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

fast_install "$1" https://www.x.org/archive/individual/lib/libXext-1.3.4.tar.gz

wget https://gitlab.freedesktop.org/mesa/mesa/-/archive/mesa-25.2.7/mesa-mesa-25.2.7.tar.gz
tar -xvf mesa-mesa-25.2.7.tar.gz
cd mesa-mesa-25.2.7

diff_patch ../../diff/mesa.diff

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dglx=xlib -Dplatforms=x11 -Dgallium-drivers=softpipe -Dllvm=false -Dvulkan-drivers= -Dvideo-codecs=all_free build
cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cp -rf "../../../glx.pc" "$1/usr/lib/pkgconfig"

cd ../..

wget https://archive.mesa3d.org/glu/glu-9.0.3.tar.xz
tar -xvf glu-9.0.3.tar.xz
cd glu-9.0.3

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dgl_provider=gl build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..


cd ..