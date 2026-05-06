. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    cp -rf "${source_dir}"/* .
}

build() {
    CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-g++ CPP=x86_64-orange-mlibc-g++ LD=x86_64-orange-mlibc-ld PKGCONFIG=x86_64-orange-mlibc-pkg-config PKG_CONFIG=x86_64-orange-mlibc-pkg-config make -j$(nproc) CFLAGS="-Wno-implicit-function-declaration -fPIC $CFLAGS -lXau -lXdmcp" LDFLAGS="-lXau -lXdmcp"
}

install() {
    mkdir -p build
    CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-g++ CPP=x86_64-orange-mlibc-g++ LD=x86_64-orange-mlibc-ld PKGCONFIG=x86_64-orange-mlibc-pkg-config PKG_CONFIG=x86_64-orange-mlibc-pkg-config make install DESTDIR="$(realpath build)"
    cp -rf build/usr/local/* "${dest_dir}/usr/"
}

pkg_work
exit