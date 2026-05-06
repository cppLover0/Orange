. "${pkg_lib}"

prepare() {
    true
    autoreconf -vfi
}

configure() {
    cp -rf  "${source_dir}"/* .
    ./configure --prefix="${host_dest_dir}" --program-suffix=-1.17
}

build() {
    make ACLOCAL=aclocal AUTOMAKE=automake -j$(nproc)
}

install() {
    make install 
}

pkg_work
exit