. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="-std=gnu17 -Wno-implicit-function-declaration" "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr --enable-widec --enable-pc-files --with-shared --with-cxx-shared --with-cxx-binding --without-normal --without-debug --with-manpage-format=normal --with-pkg-config-libdir=/usr/lib/pkgconfig --with-termlib 
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"

    for lib in ncurses ncurses++ form panel menu tinfo; do
       rm -vf                    "${dest_dir}/usr"/lib/lib${lib}.so
       echo "INPUT(-l${lib}w)" > "${dest_dir}/usr"/lib/lib${lib}.so
       ln -sfv ${lib}w.pc        "${dest_dir}/usr"/lib/pkgconfig/${lib}.pc
       patchelf --set-soname lib${lib}w.so "${dest_dir}/usr"/lib/lib${lib}w.so
    done
    rm -vf                     "${dest_dir}/usr"/lib/libcursesw.so
    echo "INPUT(-lncursesw)" > "${dest_dir}/usr"/lib/libcursesw.so
    ln -sfv libncurses.so      "${dest_dir}/usr"/lib/libcurses.so

    rm -rf "${dest_dir}/usr"/lib/*.a

}

pkg_work
exit