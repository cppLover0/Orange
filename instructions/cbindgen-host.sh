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
    cp -rf "${source_dir}"/* .
}

build() {
    true
}

install() {
    cargo install --locked --target x86_64-unknown-linux-gnu --path . --root "${host_dest_dir}"
}

pkg_work
exit