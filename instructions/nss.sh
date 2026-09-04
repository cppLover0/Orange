. "${pkg_lib}"

# i clankered it

prepare() {
    true
}

configure() {
    cd "${source_dir}/nss"
    mkdir -p out
    
    local triple="x86_64-orange-mlibc"
    
    export CC="${triple}-gcc"
    export CXX="${triple}-g++"
    export AR="${triple}-ar"
    export NM="${triple}-nm"
    export RANLIB="${triple}-ranlib"
    
    export CFLAGS="${CFLAGS} -Wno-error"
    export CXXFLAGS="${CXXFLAGS} -Wno-error"

    gyp -f ninja \
        --depth=. \
        --generator-output=out \
        -DOS=linux \
        -Dtarget_arch=x64 \
        -Dhost_arch=x64 \
        -Dwerror=0 \
        -Duse_system_sqlite=1 \
        -Ddisable_tests=1 \
        -Dsign_libs=0 \
        -Denable_sslkeylogfile=1 \
        -Dnspr_include_dir="${dest_dir}/usr/include/nspr" \
        -Dnspr_lib_dir="${dest_dir}/usr/lib" \
        -Dnss_dist_dir="${source_dir}/dist" \
        -Dnss_dist_obj_dir="${source_dir}/nss/out/Release" \
        nss.gyp
}

build() {
    cd "${source_dir}/nss"
    
    local real_build_dir="out/Release"
    if [ -f "out/out/Release/build.ninja" ]; then
        real_build_dir="out/out/Release"
    fi
    
    ninja -C "${real_build_dir}" -j$(nproc)
}

install() {
    local build_out="${source_dir}/nss/out/Release"
    if [ ! -d "${build_out}/lib" ] && [ -d "${source_dir}/nss/out/out/Release/lib" ]; then
        build_out="${source_dir}/nss/out/out/Release"
    fi
    
    mkdir -p "${dest_dir}/usr/bin" "${dest_dir}/usr/lib" "${dest_dir}/usr/include/nss"
    
    if [ -d "${build_out}/bin" ]; then
        cp -r "${build_out}/bin"/* "${dest_dir}/usr/bin/"
    fi
    
    if [ -d "${build_out}/lib" ]; then
        cp -d "${build_out}/lib"/*.so* "${dest_dir}/usr/lib/"
    fi
    
    cd "${source_dir}"
    
    if [ -d "dist/public/nss" ]; then
        cp -pL "dist/public/nss"/*.h "${dest_dir}/usr/include/nss/" 2>/dev/null || true
    elif [ -d "nss/dist/public/nss" ]; then
        cp -pL "nss/dist/public/nss"/*.h "${dest_dir}/usr/include/nss/" 2>/dev/null || true
    fi

    find nss/lib/nss nss/lib/ssl nss/lib/util nss/lib/pk12util nss/lib/certdb nss/lib/cryptohi \
         -maxdepth 1 -type f -name "*.h" -exec cp -p {} "${dest_dir}/usr/include/nss/" \; 2>/dev/null || true
    
    find nss/ -type f -name "nss.h" -exec cp -p {} "${dest_dir}/usr/include/nss/" \; 2>/dev/null || true

    mkdir -p "${dest_dir}/usr/lib/pkgconfig"
    cat << EOF > "${dest_dir}/usr/lib/pkgconfig/nss.pc"
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include/nss

Name: NSS
Description: Network Security Services
Version: 3.100
Requires: nspr >= 4.35
Libs: -L\${libdir} -lssl3 -lsmime3 -lnss3 -lnssutil3
Cflags: -I\${includedir}
EOF
}

pkg_work
exit
