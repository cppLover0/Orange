. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -lXau -lXdmcp" meson_configure -Dsystemd_user_dir=/remove_later -Dintrospection=disabled
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
    rm -rf "${dest_dir}/remove_later"
}

pkg_work
exit