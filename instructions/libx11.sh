. "${pkg_lib}"

CFLAGS=""
CXXFLAGS=""
LDFLAGS=""

prepare() {
    autotools_recursive_regen
    touch aclocal.m4 Makefile.in configure
}

configure() {
    mkdir -p "${dest_dir}/usr/include/X11"
    "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr --with-keysymdefdir="${dest_dir}/usr/include/X11" --disable-malloc0returnsnull
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit