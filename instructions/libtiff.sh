. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    cmake_configure
}

build() {
    cmake --build . -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" cmake --install .
}

pkg_work
exit