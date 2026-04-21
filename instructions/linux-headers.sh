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
    cd "${source_dir}"
    make headers_install ARCH=x86_64 INSTALL_HDR_PATH="${dest_dir}/usr"
}

pkg_work
exit