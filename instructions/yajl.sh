. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -Wno-implicit-function-declaration" cmake_configure -DCMAKE_POLICY_VERSION_MINIMUM=3.5
}

build() {
    cmake --build . -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" cmake --install .
}

pkg_work
exit