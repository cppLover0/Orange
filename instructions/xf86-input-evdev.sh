exit
. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="-fPIC" "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr  --disable-libudev
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit