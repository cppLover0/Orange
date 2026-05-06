. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -fPIC" cmake_configure -DPNG_STATIC=ON -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DZLIB_LIBRARY="${dest_dir}/usr/lib/libz.a" -DZLIB_INCLUDE_DIR="${dest_dir}/usr/include/"
}

build() {
    cmake --build . -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" cmake --install .
}

pkg_work
exit