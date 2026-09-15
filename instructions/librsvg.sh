. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    cp -f "${build_support}/config.toml" "${build_dir}/config.toml"
    meson_configure \
        -Dtriplet=x86_64-unknown-orange-mlibc \
        -Dtests=false \
        -Dpixbuf-loader=enabled
}

#idk how to cross compile cargo-c so ill just make some hack

build() {
    cd "${source_dir}"

    cargo build \
        --release \
        --target x86_64-unknown-orange-mlibc \
        --features pixbuf \
        -p librsvg-c --config "${build_support}/config.toml" --no-default-features

    cargo build \
        --release \
        --target x86_64-unknown-orange-mlibc \
        --config "${build_support}/config.toml" \
        -p rsvg_convert --bin rsvg-convert

    cargo build \
        --release \
        --target x86_64-unknown-orange-mlibc \
        --config "${build_support}/config.toml" \
        -p pixbufloader-svg

    cd "${build_dir}"
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit
