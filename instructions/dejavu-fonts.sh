. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    cp -rf "${source_dir}"/* .
}

build() {
    true
}

install() {
    mkdir -p "${dest_dir}/usr/share/fonts/truetype/dejavu"
    cp -rf ttf/* "${dest_dir}/usr/share/fonts/truetype/dejavu"
}

pkg_work
exit