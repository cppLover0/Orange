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

    mkdir -p "${dest_dir}"/usr/bin "${dest_dir}"/usr/lib

    if [ -d "${dest_dir}/lib" ] && [ ! -L "${dest_dir}/lib" ]; then
        cp -rf "${dest_dir}/lib"/* "${dest_dir}/usr/lib"
    fi

    if [ -d "${dest_dir}/lib64" ] && [ ! -L "${dest_dir}/lib64" ]; then
        cp -rf "${dest_dir}/lib64"/* "${dest_dir}/usr/lib"
    fi

    if [ -d "${dest_dir}/bin" ] && [ ! -L "${dest_dir}/bin" ]; then
        cp -rf "${dest_dir}/bin"/* "${dest_dir}/usr/bin"
    fi

    rm -rf "${dest_dir}/bin" "${dest_dir}/lib" "${dest_dir}/lib64"
    ln -s usr/lib "${dest_dir}"/lib
    ln -s usr/lib "${dest_dir}"/lib64
    ln -s usr/bin "${dest_dir}"/bin
}

pkg_work
exit