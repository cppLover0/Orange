. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    LDFLAGS="${LDFLAGS} -lstdc++ -lXau -lXdmcp" CFLAGS="${CFLAGS} -D_XOPEN_SOURCE -D_GNU_SOURCE" meson_configure -Dgles1=disabled -Dosmesa=disabled -Dlibdrm=disabled -Dx11=enabled -Dwith-system-data-files=true
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit