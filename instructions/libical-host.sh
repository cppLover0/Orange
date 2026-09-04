. "${pkg_lib}"

unset SYSROOT
unset PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH
unset PKG_CONFIG_SYSROOT_DIR
unset LLVM_CONFIG
unset VAPIGEN
unset VALAC

prepare() {
    autotools_recursive_regen
}

configure() {
    cmake -GNinja \
        -DCMAKE_INSTALL_PREFIX="${host_dest_dir}" \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="${CFLAGS}" \
        -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
        -DCMAKE_SKIP_RPATH=OFF \
        -DLIBICAL_GLIB_BUILD_DOCS=False -DLIBICAL_GLIB_VAPI=false  -DLIBICAL_JAVA_BINDINGS=OFF -DLIBICAL_GOBJECT_INTROSPECTION=false \
        "${source_dir}"
}

build() {
    cmake --build . -j$(nproc)
}

install() {
    cmake --install .
}

pkg_work
exit