. "${pkg_lib}"

prepare() {
    true
}

configure() {
    true
}

build() {
    true
}

install() {
    rm -rf "${dest_dir}/bin" "${dest_dir}/lib" "${dest_dir}/lib64"
    ln -s usr/lib "${dest_dir}"/lib
    ln -s usr/lib "${dest_dir}"/lib64
    ln -s usr/bin "${dest_dir}"/bin
    
    set +e

    x86_64-orange-mlibc-strip --strip-debug "${dest_dir}"/usr/lib/*
    x86_64-orange-mlibc-strip --strip-unneeded "${dest_dir}"/usr/bin/*
    rm -rf "${dest_dir}"/usr/{,share}/{info,man,doc}
    rm -rf "${dest_dir}"/usr/lib/*.la

}

pkg_work
exit