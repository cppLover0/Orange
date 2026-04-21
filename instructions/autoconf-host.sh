. "${pkg_lib}"

prepare() {
    true
}

configure() {
    "${source_dir}"/configure --prefix="${host_dest_dir}" MAKEINFO=true
}

build() {
    make -j$(nproc)
}

install() {
    make install -j$(nproc)
}

pkg_work
exit