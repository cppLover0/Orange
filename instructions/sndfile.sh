. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr CFLAGS="-std=gnu11" CXXFLAGS="-std=gnu++11"
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit