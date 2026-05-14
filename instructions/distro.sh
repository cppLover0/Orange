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
    rm -rf "${dest_dir}/bin" "${dest_dir}/lib" "${dest_dir}/lib64"
    ln -s usr/lib "${dest_dir}"/lib
    ln -s usr/lib "${dest_dir}"/lib64
    ln -s usr/bin "${dest_dir}"/bin
    
    set +e

    echo stripping $(find "${dest_dir}"/usr/lib/ -name "*.so") $(find "${dest_dir}"/usr/lib/ -name "*.a")
    x86_64-orange-mlibc-strip --strip-debug $(find "${dest_dir}"/usr/lib/ -name "*.so") $(find "${dest_dir}"/usr/lib/ -name "*.a")

    echo stripping "${dest_dir}"/usr/bin/*
    x86_64-orange-mlibc-strip --strip-unneeded "${dest_dir}"/usr/bin/*
    rm -rf "${dest_dir}"/usr/share/info
    rm -rf "${dest_dir}"/usr/share/man
    rm -rf "${dest_dir}"/usr/share/doc
    rm -rf "${dest_dir}"/usr/lib/*.la

    cp -rf "${distro_base_dir}"/* "${dest_dir}"

    mkdir -p "${dest_dir}/tmp"
    echo keep > "${dest_dir}"/tmp/.keep

    mkdir -p "${dest_dir}/var/log"
    echo keep > "${dest_dir}"/var/log/.keep

    mkdir -p "${dest_dir}/etc/fonts"
    echo keep > "${dest_dir}"/var/log/.keep

    mkdir -p "${dest_dir}/usr/share/gtk-3.0/"
    mkdir -p "${dest_dir}/etc/gtk-3.0"
    cp -rf "${build_support}"/settings.ini "${dest_dir}/usr/share/gtk-3.0/settings.ini"
    cp -rf "${build_support}"/settings.ini "${dest_dir}/etc/gtk-3.0/settings.ini"

    mkdir -p "${dest_dir}"/usr/lib/locale
    localedef --prefix="${dest_dir}" -i C -f UTF-8 C.UTF-8 --no-archive

    glib-compile-schemas "${dest_dir}/usr"/share/glib-2.0/schemas
    rm "${dest_dir}/usr"/share/glib-2.0/schemas/gschemas.compiled

}

pkg_work
exit