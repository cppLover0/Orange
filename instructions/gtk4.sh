. "${pkg_lib}"

export PATH="${host_dest_dir}/bin:/usr/bin"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -Wno-implicit-function-declaration -DGDK_WINDOWING_X11=1 -DHAVE_XSYNC -DHAVE_XFIXES -DHAVE_RANDR -DHAVE_XCOMPOSITE -DHAVE_XDAMAGE -DHAVE_XKB" meson_configure -Dmedia-gstreamer=disabled -Dintrospection=enabled -Dx11-backend=true -Dbroadway-backend=true -Dwayland-backend=false -Dcolord=disabled -Dvulkan=disabled -Dbuild-testsuite=false -Dbuild-tests=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit