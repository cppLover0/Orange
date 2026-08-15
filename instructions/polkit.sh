. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dlibs-only=true -Dsession_tracking=ConsoleKit -Dauthfw=shadow
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit