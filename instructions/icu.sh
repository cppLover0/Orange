. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
    cp source/config/mh-linux source/config/mh-unknown

    mkdir -p cross-build
    cd cross-build

    "${source_dir}"/source/configure --prefix=/usr

    make -j$(nproc)
    cd ..
}

configure() {
    config_data_packaging=library
    configure_script_path="${source_dir}"/source/configure "${source_dir}"/source/configure --host=x86_64-orange-mlibc --prefix=/usr --with-cross-build="${source_dir}"/cross-build --with-data-packaging=${config_data_packaging}
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit