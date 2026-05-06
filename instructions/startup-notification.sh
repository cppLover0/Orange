. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -lXau -lXdmcp" "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr lf_cv_sane_realloc=1
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit