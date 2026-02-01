
export LIBTOOL="$HOME/opt/cross/orange/bin/libtool"
export LIBTOOLIZE="$HOME/opt/cross/orange/bin/libtoolize"
export PATH="$HOME/opt/cross/orange/bin:$PATH"

sysroot_path="$(realpath initrd)"

export CFLAGS="-fPIC -Wno-error -O2 -Wno-incompatible-pointer-types"
export PKG_CONFIG_SYSROOT_DIR="$sysroot_path"
export PKG_CONFIG_PATH="$sysroot_path/usr/lib/pkgconfig:$sysroot_path/usr/share/pkgconfig:$sysroot_path/usr/local/lib/pkgconfig:$sysroot_path/usr/local/share/pkgconfig:$HOME/opt/cross/orange/lib/pkgconfig:$HOME/opt/cross/orange/share/pkgconfig"

strip --strip-debug initrd/usr/lib/*
strip --strip-unneeded initrd/usr/bin/*
rm -rf initrd/usr/{,share}/{info,man,doc}
find initrd/usr/{lib,libexec} -name \*.la -delete
