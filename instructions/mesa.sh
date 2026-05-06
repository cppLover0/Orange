. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dglx=xlib -Dplatforms=x11 -Dgallium-drivers=llvmpipe,softpipe -Dvulkan-drivers="" -Dvideo-codecs=all_free
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