. "${pkg_lib}"

unset SYSROOT
unset PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH
unset PKG_CONFIG_SYSROOT_DIR
unset LLVM_CONFIG
unset VAPIGEN
unset VALAC

prepare() {
    cd ld
    aclocal --force && automake --add-missing --copy --force-missing && autoreconf -vif
    automake --version
    automake
    cd ..

    cd libsframe
    libtoolize --force
    autoconf
    cd ..

    cd bfd
    libtoolize --force
    autoconf
    cd ..

    cd opcodes
    libtoolize --force
    autoconf
    cd ..

    cd libctf
    libtoolize --force
    autoconf
    cd ..

    cd gas
    libtoolize --force
    autoconf
    cd ..

    cd binutils
    libtoolize --force
    autoconf
    cd ..

    cd gprof
    libtoolize --force
    autoconf
    cd ..

    cd gprofng
    libtoolize --force
    autoconf
    cd ..

    autotools_recursive_regen 
}

configure() {
    "${source_dir}"/configure --target=x86_64-orange-mlibc --prefix="${host_dest_dir}" --with-sysroot="${dest_dir}" --enable-shared --disable-dependency-tracking
}

build() {
    make -j$(nproc)
}

install() {
    make install -j$(nproc)
}

pkg_work
exit