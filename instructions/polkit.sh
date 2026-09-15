. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="-Wno-implicit-function-declaration -Wno-int-conversion" meson_configure -Dsession_tracking=ConsoleKit -Dauthfw=shadow
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit