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
    cp -rf "${build_support}/cross-valac" "${build_support}/cross-vapigen" "${host_dest_dir}/bin"
}

pkg_work
exit