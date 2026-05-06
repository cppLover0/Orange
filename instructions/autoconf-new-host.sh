. "${pkg_lib}"

prepare() {
    true
}

configure() {
    "${source_dir}"/configure --prefix="${host_dest_dir}" MAKEINFO=true --program-suffix=-new
}

build() {
    make ACLOCAL=aclocal AUTOMAKE=automake -j$(nproc)
}

install() {
    make install -j$(nproc)
}

pkg_work
exit