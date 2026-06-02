. "${pkg_lib}"

unset SYSROOT
unset PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH
unset PKG_CONFIG_SYSROOT_DIR
unset LLVM_CONFIG
unset VAPIGEN
unset VALAC

prepare() {
    autotools_recursive_regen
}

configure() {
    export SYSROOT="/"
    export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
    export PKG_CONFIG_PATH=""
    export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
    CFLAGS="$(pkg-config --cflags python3)" meson_host_configure -Dbuild_introspection_data=false -Dtests=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    meson install --no-rebuild
    ln -sf g-ir-scanner "${host_dest_dir}"/bin/orange-g-ir-scanner
    ln -sf g-ir-generate "${host_dest_dir}"/bin/orange-g-ir-generate
    ln -sf g-ir-compiler "${host_dest_dir}"/bin/orange-g-ir-compiler
}

pkg_work
exit