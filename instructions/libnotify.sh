. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dintrospection=disabled \
        -Dman=false \
        -Dgtk_doc=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit