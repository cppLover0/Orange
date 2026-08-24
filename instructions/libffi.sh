. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr 
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
    ln -s libffi.so "${dest_dir}/usr/lib/libffi.so.7"
}

pkg_work
exit