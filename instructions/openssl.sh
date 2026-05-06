. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="-Wno-implicit-function-declaration" CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-gcc  "${source_dir}"/Configure --prefix=/usr --openssldir=/etc/ssl --libdir=lib "x86_64-orange-mlibc" shared zlib-dynamic no-afalgeng
}

build() {
    make -j$(nproc)
}

install() {
    make install MANSUFFIX=ssl DESTDIR="${dest_dir}"
}

pkg_work
exit