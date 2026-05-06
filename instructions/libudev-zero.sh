. "${pkg_lib}"

export CC=x86_64-orange-mlibc-gcc
export AR=x86_64-orange-mlibc-ar 
export LD=x86_64-orange-mlibc-ld
export PREFIX=/usr

prepare() {
    true
}

configure() {
    cp -rf "${source_dir}"/* .
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit