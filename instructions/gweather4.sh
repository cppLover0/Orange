. "${pkg_lib}"

unset LD_LIBRARY_PATH

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dintrospection=false -Dtests=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild

    cd "${source_dir}"

    cp -rf data/Locations.xml data/locations.dtd "${dest_dir}/usr/share/libgweather-4/"

}

pkg_work
exit