. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="-std=gnu11 -D_ALL_SOURCE -D_MLIBC_SOURCE -fcommon -D_GNU_SOURCE" "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr --without-bash-malloc --without-readline
}

build() {
    make -j$(nproc)
}

install() {
    set +e
    make install DESTDIR="${dest_dir}"
    echo if you got some error on j0 just ignore it 
}

pkg_work
exit