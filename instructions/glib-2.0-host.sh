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
    cd subprojects
    rm -rf gvdb 
    cd ..
    autotools_recursive_regen
}

configure() {
    meson_host_configure -Dintrospection=disabled
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit