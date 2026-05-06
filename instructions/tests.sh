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
    x86_64-orange-mlibc-gcc "${tests_dir}"/c_compiler_test.c -o "${dest_dir}"/usr/bin/c_compiler_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/signal_test.c -o "${dest_dir}"/usr/bin/signal_test -mno-red-zone
    x86_64-orange-mlibc-gcc "${tests_dir}"/sockets_test.c -o "${dest_dir}"/usr/bin/sockets_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/fbdev_test.c -o "${dest_dir}"/usr/bin/fbdev_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/mouse_test.c -o "${dest_dir}"/usr/bin/mouse_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/pty_test.c -o "${dest_dir}"/usr/bin/pty_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/orangedebug.c -o "${dest_dir}"/usr/bin/orangedebug
    cp -rf "${tests_dir}/launch_xorg.sh" "${dest_dir}"/usr/bin/launch_xorg.sh
    chmod +x "${dest_dir}"/usr/bin/launch_xorg.sh
}

pkg_work
exit