. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
    rm -rf subprojects/gvc
    cd subprojects
    git clone https://gitlab.gnome.org/GNOME/libgnome-volume-control.git gvc
    cd gvc 
    patch -p1 < "${tests_dir}"/../patches/gvc.diff
}

configure() {
    meson_configure -Dsystemd=false -Dgudev=false -Dwayland=false -Dsmartcard=false -Dcups=false -Drfkill=false -Dwwan=false -Dnetwork_manager=false -Dcolord=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit