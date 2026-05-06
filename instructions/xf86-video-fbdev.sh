. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
    cd "${dest_dir}"/usr/lib
    set +e
    ln -s xorg/modules/* .
    set -e
}

configure() {
    LDFLAGS="-lfbdevhw -lshadow -Wl,--allow-shlib-undefined" CFLAGS="-fPIC" SYSROOT="${dest_dir}/" "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr --disable-static --enable-shared --disable-pciaccess
}

build() {
    make ACLOCAL="aclocal" AUTOMAKE="automake"
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit