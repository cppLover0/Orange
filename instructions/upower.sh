. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure \
    -Dsystemdsystemunitdir=no \
    -Dintrospection=enabled \
    -Dgtk-doc=false \
    -Dman=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit