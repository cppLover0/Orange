
. "${pkg_lib}"

unset SYSROOT
unset PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH
unset PKG_CONFIG_SYSROOT_DIR
unset LLVM_CONFIG
unset VAPIGEN
unset VALAC
unset LD_LIBRARY_PATH

prepare() {
    true
}

configure() {
    meson --cross-file "${build_support}/mlibc-orange.cross-file" -Dheaders_only=true --prefix=/usr "${source_dir}" -Dlinux_kernel_headers="${dest_dir}/usr/include" -Dposix_option=enabled -Dlinux_option=enabled -Dglibc_option=enabled -Dbsd_option=enabled
}

build() {
    ninja
}

install() {
    DESTDIR="${dest_dir}" ninja install
}

pkg_work
exit