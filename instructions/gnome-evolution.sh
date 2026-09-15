. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    cmake_configure -DENABLE_GOA=OFF -DWITH_LIBDB=OFF -DENABLE_OAUTH2_WEBKITGTK=OFF -DENABLE_OAUTH2_WEBKITGTK4=OFF -DWITH_OPENLDAP=OFF -DWITH_NSPR_INCLUDES="${dest_dir}/usr/include/nspr" -DWITH_NSS_INCLUDES="${dest_dir}/usr/include/nss" -DWITH_KRB5=OFF
}

build() {
    cmake --build . -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" cmake --install .
    echo symlink
    rm -rf "${dest_dir}/usr/lib/libedbus-private.so"
    ln -s evolution-data-server/libedbus-private.so "${dest_dir}/usr/lib/libedbus-private.so"
}

pkg_work
exit