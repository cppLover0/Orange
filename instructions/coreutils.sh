. "${pkg_lib}"

prepare() {
    cd build-aux
    autotools_recursive_regen
}

configure() {
    "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr ACLOCAL=aclocal AUTOMAKE=automake
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit