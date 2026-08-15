. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dnative_backend=false -Dtests=false -Degl=false -Dgles2=false -Dwayland=false -Dudev=false -Dnative_backend=false -Dwayland_eglstream=false -Dsm=false -Dlibwacom=false -Dpango_ft2=false -Dremote_desktop=false -Dtests=false -Dprofiler=false 
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit