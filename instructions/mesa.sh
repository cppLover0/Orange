. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure \
        -Dglx=xlib \
        -Dplatforms=x11 \
        -Dgles1=enabled \
        -Dgles2=enabled \
        -Dgallium-drivers=llvmpipe \
        -Dvulkan-drivers="" \
        -Dvideo-codecs=all_free
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
    cp -rf "${build_support}/pcs/glx.pc" "${dest_dir}/usr/lib/pkgconfig"
}

pkg_work
exit