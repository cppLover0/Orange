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
    autoreconf -vfi
}

configure() {
    cp -rf  "${source_dir}"/* .
    ./configure --prefix="${host_dest_dir}" --program-suffix=-1.17
}

build() {
    make ACLOCAL=aclocal AUTOMAKE=automake -j$(nproc)
}

install() {
    make install 
}

pkg_work
exit