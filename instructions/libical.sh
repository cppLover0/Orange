. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

export LD=x86_64-orange-mlibc-ld 
export CC=x86_64-orange-mlibc-gcc 

configure() {

    cat << EOF > "${source_dir}/libical.cmake"

    add_executable(native-ical-glib-src-generator IMPORTED)
    set_target_properties(native-ical-glib-src-generator PROPERTIES
    IMPORTED_LOCATION "${host_dest_dir}/libexec/libical/ical-glib-src-generator")

EOF

    cmake_configure -DLIBICAL_STATIC=NO -DIMPORT_ICAL_GLIB_SRC_GENERATOR="${source_dir}"/libical.cmake -DLIBXML2_INCLUDE_DIR="${dest_dir}"/usr/include/libxml2 -DLIBXML2_LIBRARY="${dest_dir}"/usr/lib/libxml2.so  -DLIBICAL_BUILD_DOCS=false -DLIBICAL_GLIB_VAPI=false  -DLIBICAL_JAVA_BINDINGS=OFF -DLIBICAL_GOBJECT_INTROSPECTION=false 
}

build() {
    cmake --build . -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" cmake --install .
}

pkg_work
exit