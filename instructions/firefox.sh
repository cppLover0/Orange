. "${pkg_lib}"

export MOZCONFIG="${build_dir}/mozconfig"
export MOZBUILD_STATE_PATH="${build_dir}/mozbuild"
export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE="none"
export PYTHONDONTWRITEBYTECODE="1"

prepare() {
    autotools_recursive_regen
    cd third_party/rust
    rm -rf libc nix getrandom mio libloading errno rustix
    cp -rf "${sources}"/rust-libc-workdir libc
    cp -rf "${sources}"/rust-nix-workdir nix
    cp -rf "${sources}"/rust-getrandom0.3.3-workdir getrandom
    cp -rf "${sources}"/rust-mio1.0.1-workdir mio
    cp -rf "${sources}"/rust-libloading-workdir libloading
    cp -rf "${sources}"/rust-errno3.14-workdir errno
    cp -rf "${sources}"/rust-windows-link0.2.0-workdir windows-link-0.2.0
    cp -rf "${sources}"/rust-rustix0.38-workdir rustix

    sed -i 's/windows-link = { version = "^0.2"/windows-link = { version = "0.1"/g' libloading/Cargo.toml

    for crate in */; do
        if [ -d "$crate" ]; then
            echo '{"files":{}}' > "${crate}.cargo-checksum.json"
        fi
    done

    cd ../..

    sed -i '/checksum = /d' Cargo.lock

}

configure() {
    cat << EOF > "${build_dir}/mozconfig"
mk_add_options MOZ_OBJDIR="${build_dir}/obj"

ac_add_options --target=x86_64-unknown-orange-mlibc
ac_add_options --host=x86_64-pc-linux-gnu
ac_add_options --enable-application=browser
ac_add_options --prefix=/usr
ac_add_options --enable-default-toolkit=cairo-gtk3
ac_add_options --with-system-nspr

ac_add_options --enable-release
ac_add_options --disable-debug

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

ac_add_options --disable-crashreporter
ac_add_options --disable-sandbox
ac_add_options --disable-updater
ac_add_options --disable-webrtc
ac_add_options --disable-tests
ac_add_options --disable-jemalloc
ac_add_options --without-wasm-sandboxed-libraries
ac_add_options --disable-default-browser-agent

# TODO: mlibc rejects non-standard DT_* values
ac_add_options --disable-elf-hack

# fuck gecko profiler
ac_add_options --disable-gecko-profiler
ac_add_options --disable-profiling

EOF
    python3 "${source_dir}/mach" configure
}


build() {
    python3 "${source_dir}/mach" build -j$(nproc)
}

install() {
    python3 "${source_dir}/mach" package
    set -e
    rm -rf "${dest_dir}/usr/lib/firefox" "${dest_dir}/usr/bin/firefox"
    cp -a "${build_dir}/obj/dist/firefox" "${dest_dir}/usr/lib/firefox"
    ln -s ../lib/firefox/firefox "${dest_dir}/usr/bin/firefox"
}

pkg_work
exit