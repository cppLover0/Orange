. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="-Wno-implicit-function-declaration" "${source_dir}"/Configure --prefix="${host_dest_dir}" --openssldir="${host_dest_dir}/ssl" --libdir=lib linux-x86_64 shared zlib-dynamic no-afalgeng
}

build() {
    make -j$(nproc)
}

install() {
    make install MANSUFFIX=ssl
}

pkg_work
exit