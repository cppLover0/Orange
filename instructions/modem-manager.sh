. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {

    export CFLAGS="${CFLAGS} -D_GNU_SOURCE"
    export CXXFLAGS="${CXXFLAGS} -D_GNU_SOURCE"

    meson_configure -Dsystemdsystemunitdir=no \
        -Dpolkit=no \
        -Dudev=false \
        -Dudevdir=/tmp \
        -Dsystemd_journal=false \
        -Dsystemd_suspend_resume=false \
        -Dpowerd_suspend_resume=false \
        -Dintrospection=true \
        -Dmbim=false \
        -Dvapi=true \
        -Dqmi=false \
        -Dqrtr=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit