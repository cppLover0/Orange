
. "${pkg_lib}"

unset SYSROOT
unset PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH
unset PKG_CONFIG_SYSROOT_DIR
unset LLVM_CONFIG
unset VAPIGEN
unset VALAC

prepare() {
    true
}

configure() {
    cp -rf  "${source_dir}"/* .
    export CFLAGS="$CFLAGS -std=gnu89 -Dbool=glib_bool_var"
    
    if [ -f glib/m4macros/glib-gettext.m4 ]; then
        sed -i 's/m4_copy(/m4_copy_force(/g' glib/m4macros/glib-gettext.m4
    fi

    if [ -f m4macros/glib-gettext.m4 ]; then
        sed -i 's/m4_copy(/m4_copy_force(/g' m4macros/glib-gettext.m4
    fi
    
    autoreconf -fiv

    CFLAGS="$CFLAGS -Wno-implicit-function-declaration" ./configure --prefix="${host_dest_dir}" --with-internal-glib
}

build() {
    make ACLOCAL=aclocal AUTOMAKE=automake -j$(nproc)
}

install() {
    make install 
    mkdir -p "${dest_dir}"/usr/share/pkgconfig/personality.d/
    cat > "${dest_dir}"/usr/share/pkgconfig/personality.d/x86_64-orange-mlibc.personality << EOF
Triplet: x86_64-orange-mlibc
SysrootDir: ${dest_dir}
DefaultSearchPaths: ${dest_dir}/usr/lib/pkgconfig:${dest_dir}/usr/share/pkgconfig
SystemIncludePaths: ${dest_dir}/usr/include
SystemLibraryPaths: ${dest_dir}/usr/lib
EOF
}

pkg_work
exit