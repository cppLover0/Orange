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
    export CFLAGS="-fPIC -O3"
    export CXXFLAGS="-fPIC -O3"
    
    cmake -GNinja \
        -DCMAKE_INSTALL_PREFIX="${host_dest_dir}" \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_LINK_LLVM_DYLIB=ON \
        -DLLVM_TARGETS_TO_BUILD=X86 \
        -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" \
        -DDEFAULT_SYSROOT="${dest_dir}" \
        -DENABLE_LINKER_BUILD_ID=ON \
        -DLLVM_BINUTILS_INCDIR="${source_dir}/../binutils-workdir/include" \
        -DCMAKE_C_FLAGS="${CFLAGS}" \
        -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
        -DCMAKE_SKIP_RPATH=OFF \
        "${source_dir}/llvm"
}

build() {
    cmake --build . -j$(nproc)
}

install() {
    cmake --install .
}

pkg_work
exit