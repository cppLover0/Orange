. "${pkg_lib}"

export ac_cv_file__dev_ptmx=yes 
export ac_cv_file__dev_ptc=yes 
export ac_cv_func_sched_setscheduler=no 
export ac_cv_buggy_getaddrinfo=no 

prepare() {
    autotools_recursive_regen
}

configure() {
    LDFLAGS="-lintl" CFLAGS="-Os" "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr --build=x86_64 --with-computed-gotos --disable-optimizations --disable-ipv6 --without-system-expat --enable-loadable-sqlite-extensions --without-ensurepip --with-tzpath=/usr/share/zoneinfo --with-build-python=python3.13 --without-static-libpython
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit