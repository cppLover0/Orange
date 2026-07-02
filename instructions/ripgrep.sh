. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    cp -rf "${source_dir}"/* .
    true
}

build() {
    cargo build --release --target x86_64-unknown-orange-mlibc --config "${build_support}/config.toml" --no-default-features 
}

install() {
    cp -rf "${build_dir}"/target/x86_64-unknown-orange-mlibc/release/rg "${dest_dir}"/usr/bin/rg
}

pkg_work
exit