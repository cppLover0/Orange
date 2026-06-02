. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -DGDK_WINDOWING_X11=1 -DHAVE_XSYNC -DHAVE_XFIXES -DHAVE_RANDR -DHAVE_XCOMPOSITE -DHAVE_XDAMAGE -DHAVE_XKB" meson_configure -Dmedia-gstreamer=disabled -Dintrospection=enabled -Dx11-backend=true -Dbroadway-backend=true -Dwayland-backend=false -Dcolord=disabled -Dvulkan=disabled
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit