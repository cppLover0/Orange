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

    cp -rf "${distro_base_dir}"/* "${dest_dir}"

    mkdir -p "${dest_dir}/tmp"
    echo keep > "${dest_dir}"/tmp/.keep

    mkdir -p "${dest_dir}/var/log"
    echo keep > "${dest_dir}"/var/log/.keep

    mkdir -p "${dest_dir}/etc/fonts"
    echo keep > "${dest_dir}"/var/log/.keep

}

pkg_work
exit