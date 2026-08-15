. "${pkg_lib}"

prepare() {
    cd subprojects
    rm -rf gvdb 
    cd ..
    autotools_recursive_regen
}

configure() {
    meson_configure -Dglib_debug=disabled -Dman-pages=disabled -Dsysprof=disabled -Dintrospection=enabled -Dxattr=false 
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit