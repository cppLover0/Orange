. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Da11y=true -Ddebug=false -Ddocs=false -Dgir=false -Dfribidi=true -Dglade=true -Dgnutls=false -Dgtk3=true -Dgtk4=false -Dicu=true -D_systemd=false -Dvapi=false

}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit