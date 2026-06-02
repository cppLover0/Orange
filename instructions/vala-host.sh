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
    "${source_dir}"/configure --prefix="${host_dest_dir}" --disable-valadoc
}

build() {
    make -j$(nproc)
}

install() {
    make install
}

pkg_work
exit