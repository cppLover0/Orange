. "${pkg_lib}"

export CFLAGS="$(pkg-config --cflags python3) ${CFLAGS}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure 
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit