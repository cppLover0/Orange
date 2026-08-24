
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
    meson --cross-file "${build_support}/mlibc-orange.cross-file" --prefix=/usr "${source_dir}" -Duse_freestnd_hdrs=enabled -Dlinux_kernel_headers="${dest_dir}/usr/include" -Dposix_option=enabled -Dlinux_option=enabled -Dglibc_option=enabled -Dbsd_option=enabled
}

build() {
    ninja
}

install() {
    DESTDIR="${dest_dir}" ninja install
    rm -rf "${dest_dir}"/usr/lib/crt0.o
    rm -rf "${dest_dir}"/usr/lib/ld64.so.1
    ln -s ld.so "${dest_dir}"/usr/lib/ld64.so.1
    ln -s crt1.o "${dest_dir}"/usr/lib/crt0.o
    ln -s libc.so "${dest_dir}"/usr/lib/libc.so.6
}

pkg_work
exit