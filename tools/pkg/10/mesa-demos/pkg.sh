
. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

wget https://archive.mesa3d.org/demos/mesa-demos-9.0.0.tar.xz
tar -xvf mesa-demos-9.0.0.tar.xz

cd mesa-demos-9.0.0

mkdir build
meson --cross-file="$1/../tools/pkg/x86_64-orange.crossfile" --prefix=/usr -Dgles1=disabled -Dosmesa=disabled -Dlibdrm=disabled -Dx11=enabled -Dwith-system-data-files=true build

cd build

meson compile -j$(nproc)
DESTDIR="$1" meson install --no-rebuild

cd ../..

cd ..