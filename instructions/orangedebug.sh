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
    x86_64-orange-mlibc-gcc "${source_dir}"/orangedebug.c -o "${dest_dir}"/usr/bin/orangedebug
    x86_64-orange-mlibc-gcc "${source_dir}"/pidstat.c -o "${dest_dir}"/usr/bin/pidstat
}

pkg_work
exit