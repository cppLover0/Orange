. "${pkg_lib}"

unset SYSROOT
unset PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH
unset PKG_CONFIG_SYSROOT_DIR
unset LLVM_CONFIG
unset VAPIGEN
unset VALAC

export BOOTSTRAP_SKIP_TARGET_SANITY=1

prepare() {
    #autotools_recursive_regen
    export CARGO_UNSTABLE=edition2024
    cargo update --offline --manifest-path Cargo.toml
}

configure() {

    llvmconfig="$(realpath ${build_dir}/../llvm-host/bin/llvm-config)"

    cat << EOF > bootstrap.toml
              change-id = "ignore"

              [llvm]
              targets = "X86"
              download-ci-llvm = false

              [build]
              target = ["x86_64-unknown-orange-mlibc", "x86_64-unknown-linux-gnu"]
              host = ["x86_64-unknown-linux-gnu"]
              build-dir = "${build_dir}/build"
              docs = false
              compiler-docs = false

              [install]
              prefix = "${host_dest_dir}"
              sysconfdir = "etc"

              [rust]
              codegen-tests = false
              deny-warnings = false

              [target.x86_64-unknown-linux-gnu]
              llvm-config = "${llvmconfig}"
              cc = "gcc"
              cxx = "g++"

              [target.x86_64-unknown-orange-mlibc]
              llvm-config = "${llvmconfig}"
              cc = "x86_64-orange-mlibc-gcc"
              cxx = "x86_64-orange-mlibc-g++"
              ar = "x86_64-orange-mlibc-ar"
              ranlib = "x86_64-orange-mlibc-ranlib"
              linker = "x86_64-orange-mlibc-gcc"
EOF
}


build() {
    python3 "${source_dir}/x.py" build --stage 2 -j $(nproc)
}

install() {
    python3 "${source_dir}/x.py" install --stage 2 -j $(nproc)
}


pkg_work
exit