. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
    ./contrib/download_prerequisites
    cd "${source_dir}/libstdc++-v3"
    autoconf
    autotools_recursive_regen
}

configure() {
    "${source_dir}"/configure --target=x86_64-orange-mlibc --prefix="${host_dest_dir}" --with-sysroot="${dest_dir}" --enable-languages=c,c++,go --disable-nls --with-pic --enable-linker-build-id --enable-threads=posix --enable-default-pie --enable-default-ssp --disable-multilib --enable-initfini-array --enable-shared --enable-host-shared
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