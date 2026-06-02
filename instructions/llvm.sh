. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    cmake_source_dir="${source_dir}/llvm" cmake_configure -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-D_GNU_SOURCE=1 -fPIC -O3" -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE -DCMAKE_CXX_STANDARD=17 -DUNIX=1 -UBUILD_SHARED_LIBS -UENABLE_STATIC -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_LINK_LLVM_DYLIB=ON -DLLVM_ENABLE_FFI=ON -DLLVM_ENABLE_EH=ON -DLLVM_ENABLE_RTTI=ON -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-orange-mlibc -DLLVM_HOST_TRIPLE=x86_64-orange-mlibc -Wno-dev -G Ninja
}

build() {
    ninja -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" ninja install
}

pkg_work
exit