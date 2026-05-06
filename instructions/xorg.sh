. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    export LDFLAGS="-lXau -lXdmcp ${LDFLAGS}"
    meson_configure -Dxkb_bin_dir=/usr/bin -Dxkb_dir=/usr/share/X11/xkb -Dxkb_output_dir=/var/lib/xkb -Ddefault_font_path=/usr/share/fonts/X11 -Dxorg=true -Dxv=true -Dxvfb=true -Dxephyr=false -Dxnest=false -Dsuid_wrapper=false -Dpciaccess=false -Ddpms=false -Dscreensaver=true -Dxres=false -Dxvmc=false -Dsystemd_logind=false -Dsecure-rpc=false -Dudev=false -Dudev_kms=false -Ddri1=false -Ddri2=false -Ddri3=false -Dint10=false -Dvgahw=false -Ddrm=false -Dglamor=false -Dglx=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit