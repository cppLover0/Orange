. "${pkg_lib}"

export CC=x86_64-orange-mlibc-gcc
export LD=x86_64-orange-mlibc-ld 
export CXX=x86_64-orange-mlibc-g++

prepare() {
    rm -rf subprojects/gvc
    cd subprojects
    git clone https://github.com/GNOME/libgnome-volume-control.git gvc
    cd gvc 
    patch -p1 < "${tests_dir}"/../patches/gvc.diff
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -Wno-incompatible-pointer-types" LDFLAGS="-Wl,-rpath=/usr/lib/evolution-data-server -Wl,-rpath-link=/src/.orange-build/sysroot/usr/lib/evolution-data-server -Wl,-rpath=/usr/lib/mutter-10 -Wl,-rpath-link=/src/.orange-build/sysroot/usr/lib/mutter-10" meson_configure -Dgtk_doc=false -Dman=false -Dtests=false -Dnetworkmanager=false -Dsystemd=false -Dextensions_tool=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild

    echo creating symlink ../libpango-1.0.so to "${dest_dir}"/usr/lib/gnome-shell/libpango-1.0.so.0
    rm -rf "${dest_dir}"/usr/lib/gnome-shell/libpango-1.0.so.0
    ln -s ../libpango-1.0.so "${dest_dir}"/usr/lib/gnome-shell/libpango-1.0.so.0
}

pkg_work
exit