. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -Wno-maybe-uninitialized" meson_configure -Dxlib-xcb=enabled -Dzlib=enabled -Dtee=enabled -Dtests=disabled
}

build() {
    meson compile 
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit