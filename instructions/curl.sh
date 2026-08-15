. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    # TODO compile openssl 3.0.0 for curl
    "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr --without-ssl
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit