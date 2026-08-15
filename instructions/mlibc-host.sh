. "${pkg_lib}"

unset SYSROOT
unset PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH
unset PKG_CONFIG_SYSROOT_DIR
unset LLVM_CONFIG
unset VAPIGEN
unset VALAC

prepare() {
    true
}

configure() {
    CFLAGS="-D__NR_fchmodat2=452" CXXFLAGS="$CFLAGS" meson -Duse_freestnd_hdrs=disabled --cross-file "${build_support}/mlibc-linux.cross-file" --libdir=/usr/lib --prefix=/usr "${source_dir}" -Dlinux_kernel_headers="${dest_dir}/usr/include" -Dposix_option=enabled -Dlinux_option=enabled -Dglibc_option=enabled -Dbsd_option=enabled
}

build() {
    ninja
}

install() {
    mkdir -p "${host_dest_dir}/mlibc-host"
    DESTDIR="${host_dest_dir}/mlibc-host" ninja install
    rm -rf "${host_dest_dir}/mlibc-host"/usr/lib/crt0.o
    rm -rf "${host_dest_dir}/mlibc-host"/usr/lib/ld64.so.1
    ln -s ld.so "${host_dest_dir}/mlibc-host"/usr/lib/ld64.so.1
    ln -s crt1.o "${host_dest_dir}/mlibc-host"/usr/lib/crt0.o
}

pkg_work
exit