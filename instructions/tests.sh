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
    x86_64-orange-mlibc-g++ "${tests_dir}"/getuid_test.c -o "${dest_dir}"/usr/bin/getuid_test \
    -D_GLIBCXX_ASSERTIONS -fstack-protector-strong -fstrict-flex-arrays=1 \
  -DDEBUG=1 -DXPCOM_GLUE -DMOZ_HAS_MOZGLUE -DMOZILLA_CLIENT \
  -fno-rtti -pthread -fno-sized-deallocation -fno-aligned-new \
  -ffunction-sections -fdata-sections -fno-math-errno -fno-exceptions \
  -pipe -fPIC -D_GNU_SOURCE -gdwarf-4 -fno-omit-frame-pointer \
  -funwind-tables -Wall -ffp-contract=off \
  -rdynamic \
  -Wl,--build-id=sha1 -Wl,-z,noexecstack -Wl,-z,text -Wl,-z,relro -Wl,-z,now -Wl,-z,nocopyreloc


    x86_64-orange-mlibc-gcc "${tests_dir}"/total_mem_test.c -o "${dest_dir}"/usr/bin/total_mem_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/shm_test.c -o "${dest_dir}"/usr/bin/shm_test
    x86_64-orange-mlibc-gcc "${tests_dir}"/setitimer_test.c -o "${dest_dir}/usr/bin/setitimer_est"
    x86_64-orange-mlibc-gcc "${tests_dir}"/epoll_test.c -o "${dest_dir}/usr/bin/epoll_test"
    x86_64-orange-mlibc-gcc "${tests_dir}"/map_shared_test.c -o "${dest_dir}/usr/bin/map_shared_test"
    x86_64-orange-mlibc-gcc "${tests_dir}"/sqlite_test.c -o "${dest_dir}/usr/bin/sqlite_test" -lsqlite3
    clang --target=x86_64-orange-mlibc "${tests_dir}"/c_compiler_test.c -o "${dest_dir}"/usr/bin/clang_test 
    sh "${build_support}"/run-wrapper "${dest_dir}"/usr/bin/getuid_test

    echo say gex
    ulimit -c unlimited
    sh "${build_support}"/run-wrapper "${dest_dir}"/usr/bin/firefox
}
pkg_work
exit