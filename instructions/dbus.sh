. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -lXau -lXdmcp" meson_configure -Druntime_dir=/run -Dsystemd_system_unitdir=no -Dsystemd_user_unitdir=no -Dsystem_pid_file=/run/dbus/pid -Dsystem_socket=/run/dbus/system_bus_socket -Dselinux=disabled -Dapparmor=disabled -Dlibaudit=disabled -Dkqueue=disabled -Dlaunchd=disabled -Dsystemd=disabled -Dmodular_tests=disabled -Depoll=disabled -Dverbose_mode=true
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
    mkdir -p "${dest_dir}/usr"/share/dbus-1/session.d/
    mkdir -p "${dest_dir}"/var/lib/dbus/
    touch "${dest_dir}"/var/lib/dbus/.keep
    touch "${dest_dir}/usr"/share/dbus-1/session.d/.keep
}

pkg_work
exit