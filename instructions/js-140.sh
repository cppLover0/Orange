. "${pkg_lib}"

export MOZCONFIG="${build_dir}/mozconfig-js"
export MOZBUILD_STATE_PATH="${build_dir}/mozbuild"
export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE="none"
export PYTHONDONTWRITEBYTECODE="1"

prepare() {
    true
}

configure() {
cat << EOF > "${build_dir}/mozconfig-js"
ac_add_options --prefix=/usr

ac_add_options --target=x86_64-unknown-orange-mlibc
ac_add_options --host=x86_64-pc-linux-gnu

ac_add_options --enable-application=js

ac_add_options --disable-jemalloc
ac_add_options --with-intl-api
ac_add_options --with-system-zlib

ac_add_options --enable-release
ac_add_options --disable-debug
ac_add_options --with-system-nspr

export CFLAGS="-D_GNU_SOURCE"
export CXXFLAGS="-D_GNU_SOURCE"

CC="x86_64-orange-mlibc-gcc"
CXX="x86_64-orange-mlibc-g++"
AR="x86_64-orange-mlibc-ar"
NM="x86_64-orange-mlibc-nm"
RANLIB="x86_64-orange-mlibc-ranlib"
HOST_CC="gcc"
HOST_CXX="g++"
export PKG_CONFIG="pkg-config"

ac_add_options --with-libclang-path="${host_dest_dir}/lib/"
ac_add_options --with-clang-path="${host_dest_dir}/bin/clang"
export LIBCLANG_PATH="${host_dest_dir}/lib/"

export BINDGEN_CFLAGS="-target x86_64-orange-mlibc --gcc-toolchain=x86_64-orange-mlibc-gcc -I${host_dest_dir}/x86_64-orange-mlibc/include/c++/15.1.0 -I${host_dest_dir}/x86_64-orange-mlibc/include/c++/15.1.0/x86_64-orange-mlibc -I${host_dest_dir}/x86_64-orange-mlibc/include -Wno-invalid-constexpr"

export RUSTC="${host_dest_dir}/bin/rustc"
export CARGO="${host_dest_dir}/bin/cargo"
export CBINDGEN=cbindgen
export NODEJS=nodejs

ac_add_options --disable-tests
EOF
    cd "${source_dir}/js/src"

    python3 ../../mach configure
}


build() {
    cd "${source_dir}/js/src"
    python3 ../../mach build -j$(nproc)
}

install() {
    cd "${source_dir}/js/src"
    export DESTDIR="${dest_dir}"
    python3 ../../mach install
}

pkg_work
exit