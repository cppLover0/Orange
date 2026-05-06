. "${pkg_lib}"

prepare() {
    cd subprojects
    rm -rf gvdb 
    cd ..
    autotools_recursive_regen
}

configure() {
    meson_configure -Dvapi=false -Dman=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit