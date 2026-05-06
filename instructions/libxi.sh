. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr --disable-malloc0returnsnull
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit