. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
    touch configure.ac aclocal.m4 configure Makefile.am Makefile.in config.h.in

}

configure() {
    "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr 
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit