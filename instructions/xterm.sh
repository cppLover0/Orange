. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    CFLAGS="$CFLAGS -DUSE_POSIX_TERMIOS -DUSE_SYSV_PGRP -D__MLIBC_XOPEN -DHAVE_WCHAR_H" "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr 
}

build() {
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit