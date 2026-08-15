. "${pkg_lib}"

unset SYSROOT
unset PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH
unset PKG_CONFIG_SYSROOT_DIR
unset LLVM_CONFIG
unset VAPIGEN
unset VALAC

prepare() {
     autoreconf -fiv
    autotools_recursive_regen 
}

configure() {
    "${source_dir}"/configure --target=x86_64-orange-mlibc --prefix="${host_dest_dir}" --with-sysroot="${dest_dir}" --enable-shared --disable-dependency-tracking
}

build() {
    make -j$(nproc)
}

install() {
    make install
}

pkg_work
exit