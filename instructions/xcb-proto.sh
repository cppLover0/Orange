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
    sed -i "1i pc_sysrootdir=${dest_dir}" $(find "${dest_dir}/usr" -name "xcb-proto.pc")
}

pkg_work
exit