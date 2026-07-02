
trap "exit -1" USR1
cd "${build_dir}"

export LD_LIBRARY_PATH="${host_dest_dir}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

export SYSROOT="${dest_dir}"
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PKG_CONFIG_PATH=""
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export LIBTOOL=libtool
export LLVM_CONFIG="${build_support}/cross-llvm-config"
export INTROSPECTION_SCANNER_ENV="${host_dest_dir}/bin/g-ir-scanner"
export INTROSPECTION_COMPILER_ENV="${host_dest_dir}/bin/g-ir-compiler"
export INTROSPECTION_GENERATE_ENV="${host_dest_dir}/bin/g-ir-generate"

cp -rf "${build_support}/cross-llvm-config" "${host_dest_dir}/bin/llvm-config"
chmod +x "${host_dest_dir}/bin/llvm-config"

chmod +x "${build_support}/cross-valac"
chmod +x "${build_support}/cross-vapigen"

export VAPIGEN="${build_support}/cross-vapigen"
export VALAC="${build_support}/cross-valac"
export VALADIR="${dest_dir}/usr/share/vala/vapi"

#no libtool :)
find "${dest_dir}" -name "*.la" -delete

set -e

cat << EOF > "${build_support}/config.toml"
[build]
rustc = "${host_dest_dir}/bin/rustc"

[target.x86_64-unknown-orange-mlibc]
linker = "x86_64-orange-mlibc-gcc"

[patch.crates-io]
backtrace = { path = "${sources}/rust-backtrace-workdir" }
libc = { path = "${sources}/rust-libc-workdir" }
getrandom-03 = { path = "${sources}/rust-getrandom3-workdir", package = "getrandom" }
getrandom-04 = { path = "${sources}/rust-getrandom4-workdir", package = "getrandom" }
rustix = { path = "${sources}/rust-rustix-workdir", package = "rustix" }
rustix-038 = { path = "${sources}/rust-rustix0.38-workdir", package = "rustix" }
nix-030 = { path = "${sources}/rust-nix-workdir", package = "nix" }
nix-029 = { path = "${sources}/rust-nix0.29-workdir", package = "nix" }
nix-028 = { path = "${sources}/rust-nix0.28-workdir", package = "nix" }
nix-0271 = { path = "${sources}/rust-nix0.27.1-workdir", package = "nix" }
mio-120 = { path = "${sources}/rust-mio1.2.0-workdir", package = "mio" }
socket2 = { path = "${sources}/rust-socket2-workdir", package = "socket2" }
socket2-0510 = { path = "${sources}/rust-socket2-0.5.10-workdir", package = "socket2" }
tokio1521 = { path = "${sources}/rust-tokio1.52.1-workdir/tokio", package = "tokio" }
target-lexicon = { path = "${sources}/rust-targetlexicon-workdir", package = "target-lexicon" }
scap = { path = "${sources}/rust-zed-scap-workdir", package = "zed-scap" }
treesitter = { path = "${sources}/tree-sitter-workdir", package = "tree-sitter" }
wasmtime = { path = "${sources}/rust-wasmtime-workdir/crates/wasmtime", package = "wasmtime" }
awslcsys = { path = "${sources}/rust-aws-lc-sys-workdir/aws-lc-sys", package = "aws-lc-sys" }
ring = { path = "${sources}/rust-ring-workdir", package = "ring" }
libloading = { path = "${sources}/rust-libloading-workdir", package = "libloading" }
sysinterface = { path = "${sources}/rust-sysinterface-workdir", package = "system-interface" }
cap-primitives = { path = "${sources}/rust-cap-primitives-workdir", package = "cap-primitives" }
ipchcannel = { path = "${sources}/rust-ipc-channel-workdir", package = "ipc-channel" }
open = { path = "${sources}/rust-open-workdir", package = "open" }
polling = { path = "${sources}/rust-polling-workdir", package = "polling" }
errno = { path = "${sources}/rust-errno2.8-workdir", package = "errno" }
errno2 = { path = "${sources}/rust-errno3.14-workdir", package = "errno" }
EOF

im_exit() {
    kill -s USR1 $$
}

gir_patch_configure() {
    sed -i '/if test "x\$found_introspection" = "xyes"; then/,/INTROSPECTION_GENERATE=/ {
        s|INTROSPECTION_SCANNER=.*|INTROSPECTION_SCANNER="$INTROSPECTION_SCANNER_ENV"|
        s|INTROSPECTION_COMPILER=.*|INTROSPECTION_COMPILER="$INTROSPECTION_COMPILER_ENV"|
        s|INTROSPECTION_GENERATE=.*|INTROSPECTION_GENERATE="$INTROSPECTION_GENERATE_ENV"|
    }' ./configure
}

chmod +x "${build_support}"/ldd-wrapper
chmod +x "${build_support}"/run-wrapper
cp -rf "${build_support}"/ldd-wrapper "${build_support}"/run-wrapper "${host_dest_dir}/bin"

export RUN_WRAPPER_LD_LIBRARY_PATH="${host_dest_dir}/mlibc-host/usr/lib:${dest_dir}/usr/lib"
export RUN_WRAPPER_INTERP="${host_dest_dir}/mlibc-host/usr/lib/ld.so"
export GI_LDD_WRAPPER="ldd-wrapper"
export GI_CROSS_LAUNCHER="run-wrapper"
export GI_GIR_PATH="${dest_dir}/usr/share/gir-1.0"

gir_prepare() {
    true
}

pkg_work() {

    if [ "${action}" = "prepare" ]; then
        cd "${source_dir}"
        touch ./*
        prepare
    fi

    if [ "${action}" = "configure" ]; then
        configure
    fi

    if [ "${action}" = "build" ]; then
        build
    fi

    if [ "${action}" = "install" ]; then
        install
    fi

}

checked_subst() {
    tmpfile="$2".checked_subst
    sed -z -E -e "$1" "$2" >"$tmpfile"
    if cmp -s "$2" "$tmpfile"; then
        rm -f "$2".checked_subst
        if [ "$3" = no_die ]; then
            return 1
        else
            die "*** substitution '$1' failed for file '$2'"
        fi
    fi

    #diff --color=auto -ur "$2" "$tmpfile" || true

    touch -r "$2" "$2".checked_subst
    chmod --reference="$2" "$2".checked_subst
    mv -f "$2".checked_subst "$2"
}

autotools_recursive_regen() {
    for f in $(grep -rl 'GNU config.sub ($timestamp)'); do
        mv "$f" "$f".reference
        cp -v "${build_support}/config.sub" "$f"
        touch -r "$f".reference "$f"
        rm -f "$f".reference
    done
    for f in $(grep -rl 'GNU config.guess ($timestamp)'); do
        mv "$f" "$f".reference
        cp -v "${build_support}/config.guess" "$f"
        touch -r "$f".reference "$f"
        rm -f "$f".reference
    done

    if ! [ -z "$(grep -rl "# No shared lib support for Linux oldld, aout, or coff.")" ]; then
        if [ -z "$(grep -rl "dynamic_linker='mlibc ld.so'")" ]; then
            echo "*** Missing libtool support for mlibc - trying to patch support in :3 ***"
            for f in $(grep -rl "We cannot seem to hardcode it, guess we'll fake it."); do
                if grep -q 'add_dir="\?-L$lt_sysroot$libdir"\?' "$f"; then
                    continue
                fi
                checked_subst 's/add_dir=(")?-L\$libdir(")?/add_dir=\1-L$lt_sysroot$libdir\1/g' "$f"
            done
            for f in $(grep -rl "# No shared lib support for Linux oldld, aout, or coff."); do
                if grep -q 'AC_DEFUN(\[AC_PROG_LIBTOOL\]' "$f"; then
                    continue
                fi
                if grep -q 'ltconfig - Create a system-specific libtool' "$f"; then
                    continue
                fi
                checked_subst 's/(# This must be (glibc\/|Linux )?ELF.\nlinux\* \| k\*bsd\*-gnu \| kopensolaris\*-gnu( \| gnu\*)?( \| uclinuxfdpiceabi)?)(\)\n  lt_cv_deplibs_check_method=pass_all)/\1 | *-mlibc\5/g' "$f"
                checked_subst 's/(\)\n	# FreeBSD uses GNU C)/ | *-mlibc\1/g' "$f" no_die || true
                checked_subst 's/(lt_prog_compiler_static(_[^=]*)?='"'"'-non_shared'"'"'\n      ;;)(\n\n    linux\* \| k\*bsd\*-gnu \| kopensolaris\*-gnu( \| gnu\*)?\))/\1\n\n    *-mlibc)\n      lt_prog_compiler_wl\2='"'"'-Wl,'"'"'\n      lt_prog_compiler_pic\2='"'"'-fPIC'"'"'\n      lt_prog_compiler_static\2='"'"'-static'"'"'\n      ;;\3/g' "$f"
                checked_subst 's/(    (haiku|interix\[3-9\])?\*\)\n      (archive_cmds|hardcode_direct)?(_[^=]*)?=)/    *-mlibc)\n      archive_cmds\4='"'"'$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'"'"'\n      archive_expsym_cmds\4='"'"'$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'"'"'\n      ;;\n\n\1/g' "$f"
                checked_subst 's/(\)\n        # FreeBSD 3 and later use GNU C)/ | *-mlibc\1/g' "$f" no_die || true
                # putting this last to avoid a bug with determining whether the substitutions should be run or not.
                checked_subst 's/(hardcode_into_libs=yes\n  ;;\n\n)(# No shared lib support for Linux oldld, aout, or coff.)/\1*-mlibc)\n  version_type=linux\n  need_lib_prefix=no\n  need_version=no\n  library_names_spec='"'"'$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'"'"'\n  soname_spec='"'"'$libname$release$shared_ext$major'"'"'\n  dynamic_linker='"'"'mlibc ld.so'"'"'\n  shlibpath_var=LD_LIBRARY_PATH\n  shlibpath_overrides_runpath=no\n  hardcode_into_libs=yes\n  ;;\n\n\2/g' "$f"
            done
        fi
    fi
}

# i stole something from jinxfiles cuz why not

meson_configure() {
    meson_configure_noflags "$@"
}

meson_configure_noflags() {
    if [ -z "${meson_source_dir}" ]; then
        meson_source_dir="${source_dir}"
    fi

    meson setup "${meson_source_dir}" \
        --cross-file "${build_support}/x86_64-orange.crossfile" \
        --prefix=/usr \
        --sysconfdir=/etc \
        --localstatedir=/var \
        --libdir=lib \
        --sbindir=bin \
        --native-file "${build_support}/native-file-meson" \
        --buildtype=release \
        -Ddefault_library=shared \
        "$@"
}

meson_host_configure() {
    meson_host_configure_noflags "$@"
}

meson_host_configure_noflags() {
    if [ -z "${meson_source_dir}" ]; then
        meson_source_dir="${source_dir}"
    fi

    meson setup "${meson_source_dir}" \
        --prefix="${host_dest_dir}" \
        --sysconfdir=/etc \
        --localstatedir=/var \
        --libdir=lib \
        --sbindir=bin \
        --native-file "${build_support}/native-file-meson" \
        --buildtype=release \
        -Ddefault_library=shared \
        "$@"
}

cmake_configure() {
    cmake_configure_noflags \
        "$@"
}

cmake_configure_noflags() {
    if [ -z "${cmake_source_dir}" ]; then
        cmake_source_dir="${source_dir}"
    fi

    cmake "${cmake_source_dir}" \
        -DCMAKE_TOOLCHAIN_FILE="${build_support}/x86_64-orange.cmake" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_SYSCONFDIR=/etc \
        -DCMAKE_INSTALL_LOCALSTATEDIR=/var \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_INSTALL_SBINDIR=bin \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DBUILD_STATIC_LIBS=OFF \
        -DENABLE_STATIC=OFF \
        -DCMAKE_COLOR_DIAGNOSTICS=ON \
        -GNinja \
        "$@"
}

export CFLAGS="-O3"
export CXXFLAGS="$CFLAGS"