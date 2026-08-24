. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

gir_cross_env() {
    export CC=x86_64-orange-mlibc-gcc
    export LDSHARED="x86_64-orange-mlibc-gcc -shared"
    export LD=x86_64-orange-mlibc-ld
    export LDFLAGS="-Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-all"
    export CFLAGS="$(pkg-config --cflags python3) ${LDFLAGS}"
}

configure() {
    gir_cross_env
    meson_configure -Dbuild_introspection_data=true -Dtests=false -Dgi_cross_pkgconfig_sysroot_path="${dest_dir}" -Dgir_scanner="${host_dest_dir}/bin/g-ir-scanner"
}

build() {
    gir_cross_env
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit
