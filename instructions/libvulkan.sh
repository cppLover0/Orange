. "${pkg_lib}"

export VULKAN_HEADERS_INSTALL_DIR="${dest_dir}/usr/include"

prepare() {
    autotools_recursive_regen
}

configure() {
    export CFLAGS="-DFALLBACK_CONFIG_DIRS=\"/etc/xdg\" -DFALLBACK_DATA_DIRS=\"/usr/local/share:/usr/share\" -DSYSCONFDIR=\"/etc\""
    cmake_configure -DVULKAN_HEADERS_INSTALL_DIR="${dest_dir}"/usr 
}

build() {
    cmake --build . -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" cmake --install .
}

pkg_work
exit