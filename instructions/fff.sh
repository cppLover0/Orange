. "${pkg_lib}"

export CC=x86_64-orange-mlibc-gcc
export LD=x86_64-orange-mlibc-ld

prepare() {
    cp -rf "${source_dir}"/* "${build_dir}"
}

configure() {
    true
}

build() {
    make -j$(nproc)
}

install() {
    cp -rf flancon "${dest_dir}"/usr/bin/flancon
}

pkg_work
exit