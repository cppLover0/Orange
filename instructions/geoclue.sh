. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dnmea-source=false -Dgtk-doc=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit