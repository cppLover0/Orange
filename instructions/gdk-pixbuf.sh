. "${pkg_lib}"

gir_prepare

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dgio_sniffing=true -Dman=false -Dintrospection=enabled -Dbuiltin_loaders=all
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit