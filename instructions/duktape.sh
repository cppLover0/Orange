. "${pkg_lib}"

prepare() {
    cp -rf "${source_dir}"/* "${build_dir}"
}

configure() {
    true
}

build() {
    x86_64-orange-mlibc-gcc -shared -fPIC -Wno-implicit-function-declaration -O2 src/duktape.c \
        -o libduktape.so.2.7.0 -lm -Wl,-soname,libduktape.so.2

    cat << 'EOF' > duktape.pc
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: duktape
Description: Embeddable Javascript engine
Version: 2.7.0
Libs: -L${libdir} -lduktape
Cflags: -I${includedir}
EOF
}

install() {
    mkdir -p "${dest_dir}/usr/include"
    mkdir -p "${dest_dir}/usr/lib/pkgconfig"

    cp src/duktape.h src/duk_config.h "${dest_dir}/usr/include/"

    cp libduktape.so.2.7.0 "${dest_dir}/usr/lib/"
    ln -s libduktape.so.2.7.0 "${dest_dir}/usr/lib/libduktape.so.2"
    ln -s libduktape.so.2.7.0 "${dest_dir}/usr/lib/libduktape.so"

    cp duktape.pc "${dest_dir}/usr/lib/pkgconfig/"
}

pkg_work
exit
