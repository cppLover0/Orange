. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dvapi=enabled -Dintrospection=enabled -Dgssapi=disabled -Dsysprof=disabled -Dtests=false -Dtls_check=false
}

build() {
    meson compile -j$(nproc) 
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit