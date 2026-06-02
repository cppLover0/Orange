. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dgraphite2=disabled -Ddocs=disabled -Dglib=enabled -Dgobject=enabled -Dicu=disabled -Dfreetype=enabled -Dcairo=enabled -Dintrospection=enabled -Dtests=disabled
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit