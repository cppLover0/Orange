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
    true
}

build() {
    make all-target-libstdc++-v3
}

install() {
    make install-target-libstdc++-v3 
    cp -rf "${host_dest_dir}"/x86_64-orange-mlibc/lib/libstdc++.so* "${dest_dir}/usr/lib"
}

pkg_work
exit