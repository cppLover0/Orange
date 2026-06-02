. "${pkg_lib}"

prepare() {
    true
}

configure() {
    true
}

build() {
    true
}

install() {
    cp -rf "${source_dir}/launch_xorg.sh" "${dest_dir}"/usr/bin/launch_xorg.sh
    chmod +x "${dest_dir}"/usr/bin/launch_xorg.sh
}

pkg_work
exit