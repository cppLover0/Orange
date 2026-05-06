. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="-I\"${dest_dir}\"/usr/include/freetype2 -Wno-implicit-function-declaration" meson_configure 
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit