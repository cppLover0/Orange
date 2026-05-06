. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    LDFLAGS="-lXau -lXdmcp" meson_configure 
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit