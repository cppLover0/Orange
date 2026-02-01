
sudo rm -rf initrd

export LIBTOOL="$HOME/opt/cross/orange/bin/libtool"
export LIBTOOLIZE="$HOME/opt/cross/orange/bin/libtoolize"
export PATH="$HOME/opt/cross/orange/bin:$PATH"

sysroot_path="$(realpath initrd)"

export CFLAGS="-fPIC -Wno-error -O2 -Wno-incompatible-pointer-types -Wno-implicit-function-declaration"
export PKG_CONFIG_SYSROOT_DIR="$sysroot_path"
export PKG_CONFIG_PATH="$sysroot_path/usr/lib/pkgconfig:$sysroot_path/usr/share/pkgconfig:$sysroot_path/usr/local/lib/pkgconfig:$sysroot_path/usr/local/share/pkgconfig:$HOME/opt/cross/orange/lib/pkgconfig:$HOME/opt/cross/orange/share/pkgconfig"

cd tools/pkg
bash build-pkg.sh "$(realpath ../../initrd)"
cd ../..

cd initrd

rm -rf lib lib64 bin

ln -sf usr/lib lib
ln -sf usr/lib lib64
ln -sf usr/bin bin

cd ../

cp -rf tools/initbase/* initrd

mkdir -p tools/base/boot
tar -cf tools/base/boot/initrd.tar -C initrd .
