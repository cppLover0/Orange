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
    x86_64-orange-mlibc-gcc "${tests_dir}"/dup2_test.c -o "${dest_dir}"/usr/bin/dup2_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/uid_to_hex_test.c -o "${dest_dir}"/usr/bin/uid_to_hex_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/so_credentials_test.c -o "${dest_dir}"/usr/bin/so_credentials_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/getuid_test.c -o "${dest_dir}"/usr/bin/getuid_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/total_mem_test.c -o "${dest_dir}"/usr/bin/total_mem_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/shm_test.c -o "${dest_dir}"/usr/bin/shm_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/setitimer_test.c -o "${dest_dir}/usr/bin/setitimer_est"
    x86_64-orange-mlibc-gcc "${tests_dir}"/epoll_test.c -o "${dest_dir}/usr/bin/epoll_test"
    clang --target=x86_64-orange-mlibc "${tests_dir}"/c_compiler_test.c -o "${dest_dir}"/usr/bin/clang_test
    rustc "${tests_dir}/rust_test.rs" --target x86_64-unknown-orange-mlibc -C linker=x86_64-orange-mlibc-gcc -o "${dest_dir}/usr/bin/rust_test"
}

pkg_work
exit