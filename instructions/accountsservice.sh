. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -Wno-implicit-function-declaration" meson_configure \
        -Dsystemdsystemunitdir=no \
        -Dsystemd=false \
        -Delogind=false \
        -Dgtk_doc=false \
        -Dadmin_group=wheel
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit
