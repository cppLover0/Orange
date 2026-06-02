. "${pkg_lib}"

prepare() {
    sed -i -r 's:"(/system):"/org/gnome\1:g' schemas/*.in
    autotools_recursive_regen
}

configure() {
    meson_configure -Dintrospection=true
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
    glib-compile-schemas "${dest_dir}/usr"/share/glib-2.0/schemas
    rm "${dest_dir}/usr"/share/glib-2.0/schemas/gschemas.compiled
}

pkg_work
exit