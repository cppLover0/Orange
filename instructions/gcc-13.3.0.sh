. "${pkg_lib}"

prepare() {
    ./contrib/download_prerequisites
}

configure() {
    "${source_dir}"/configure --prefix="${host_dest_dir}" --enable-threads=posix --enable-languages=c,c++,lto --enable-shared --enable-default-pie --disable-multilib
}

build() {
    make all-gcc -j$(nproc)
    make all-target-libgcc -j$(nproc)
}

install() {
    make install-gcc -j$(nproc)
    make install-target-libgcc -j$(nproc)
    make all-target-libstdc++-v3
    make install-target-libstdc++-v3 
}

pkg_work
exit