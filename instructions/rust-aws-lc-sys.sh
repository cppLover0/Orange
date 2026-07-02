. "${pkg_lib}"

prepare() {
    cp -rf "${sources}"/rust-aws-lc/* "${source_dir}"/aws-lc-sys/aws-lc
}

configure() {
    true
}

build() {
    true
}

install() {
    true
}

pkg_work
exit