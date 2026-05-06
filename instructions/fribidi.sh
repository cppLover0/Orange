. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() { 
    export CFLAGS="$CFLAGS -DHAVE_GETOPT -DHAVE_GETENV -D__GNU_LIBRARY__"
    meson_configure -Ddocs=false -Dtests=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit