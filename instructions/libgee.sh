. "${pkg_lib}"

prepare() {
    gir_patch_configure
    autotools_recursive_regen
}

configure() {
    "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr --enable-introspection INTROSPECTION_COMPILER=/usr/bin/g-ir-compiler
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit