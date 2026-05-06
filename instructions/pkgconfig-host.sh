
. "${pkg_lib}"

prepare() {
    true
}

configure() {
    cp -rf  "${source_dir}"/* .
    export CFLAGS="$CFLAGS -std=gnu89 -Dbool=glib_bool_var"
    autoreconf -fi
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