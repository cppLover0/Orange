. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CHOST=x86_64-orange-mlibc prefix="/usr" "${source_dir}"/configure --static
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit