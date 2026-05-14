. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CC=x86_64-orange-mlibc-gcc "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr --enable-static=no --enable-shared=yes --enable-thread-safe --with-pic
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit