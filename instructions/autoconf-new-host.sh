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
    "${source_dir}"/configure --prefix="${host_dest_dir}" MAKEINFO=true --program-suffix=-new
}

build() {
    make ACLOCAL=aclocal AUTOMAKE=automake -j$(nproc)
}

install() {
    make install -j$(nproc)
}

pkg_work
exit