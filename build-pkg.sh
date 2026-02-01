
sysroot_path="$(realpath initrd)"

export PATH="$HOME/opt/cross/orange/bin:$PATH"

export PKG_CONFIG_SYSROOT_DIR="$sysroot_path"
export PKG_CONFIG_PATH="$sysroot_path/usr/lib/pkgconfig:$sysroot_path/usr/share/pkgconfig:$sysroot_path/usr/local/lib/pkgconfig:$sysroot_path/usr/local/share/pkgconfig:$HOME/opt/cross/orange/lib/pkgconfig:$HOME/opt/cross/orange/share/pkgconfig"
INITRDDIR=""$(realpath initrd)""

export CFLAGS="-fPIC -Wno-error -O3 -Wno-array-bounds -Wno-int-conversion -Wno-incompatible-pointer-types  -Wno-implicit-function-declaration -fno-omit-frame-pointer -ggdb3 -fno-inline-functions-called-once -fno-optimize-sibling-calls  -fno-omit-frame-pointer"
export CXXFLAGS="$CFLAGS"

cd tools/pkg/$1 
sh pkg.sh "$INITRDDIR"
cd ../../../../

mkdir -p tools/base/boot
sudo tar -cf tools/base/boot/initrd.tar -C initrd .
