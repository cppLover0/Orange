. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    cp -rf "${source_dir}"/* .
    true
}

build() {
    export LK_CUSTOM_WEBRTC="/dev/null"
    export ZED_BUNDLE_LIBWEBRTC=false
    cargo build --target x86_64-unknown-orange-mlibc --config "${build_support}/config.toml" 
}

install() {
    cp -rf "${build_dir}"/target/x86_64-unknown-orange-mlibc/debug/zed "${dest_dir}"/usr/bin/zed
}

pkg_work
exit