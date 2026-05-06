. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -DHAVE_XSYNC -DHAVE_XFIXES -DHAVE_RANDR -DHAVE_XCOMPOSITE -DHAVE_XDAMAGE -DHAVE_XKB" meson_configure -Dprint_backends=file -Dintrospection=false -Dx11_backend=true -Dbroadway_backend=true -Dwayland_backend=false -Dcolord=no
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit