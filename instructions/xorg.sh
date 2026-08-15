. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    export LDFLAGS="-lXau -lXdmcp ${LDFLAGS}"
    export CFLAGS="${CFLAGS} -Wno-implicit-function-declaration"
    meson_configure -Dxkb_bin_dir=/usr/bin -Dxkb_dir=/usr/share/X11/xkb -Dxkb_output_dir=/var/lib/xkb -Ddefault_font_path=/usr/share/fonts/X11 -Dxorg=true -Dxv=true -Dxvfb=true -Dxephyr=false -Dxnest=false -Dsuid_wrapper=false -Dpciaccess=true -Ddpms=true -Dscreensaver=true -Dxres=true -Dxvmc=false -Dsystemd_logind=false -Dsecure-rpc=false -Dudev=false -Dudev_kms=false -Ddri1=true -Ddri2=true -Ddri3=true -Dint10=false -Dvgahw=false -Ddrm=true -Dglamor=false -Dglx=true
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit