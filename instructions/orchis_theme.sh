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
    cd "${source_dir}"
    /usr/bin/install -dm 755 "${dest_dir}/usr/share/themes"
	./install.sh --tweaks submenu --dest "${dest_dir}/usr/share/themes"
}

pkg_work
exit