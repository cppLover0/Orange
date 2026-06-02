. "${pkg_lib}"

gir_prepare

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dgir=true -Dvapi=true
}

build() {
    meson compile
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit