
GNU_MIRROR=https://mirror.dogado.de/

cachedownload() {
    if [ ! -f "../cached/$1" ]; then
        curl -o ../cached/$1 $2
    fi
}

installgnu() {
    cachedownload ../cached/$2-$3.tar.gz "$GNU_MIRROR/gnu/$1/$2-$3.tar.gz"
    rm -rf ./*
    cp -rf ../cached/$2-$3.tar.gz .
    tar -xvf $2-$3.tar.gz
}

diff_patch() {
    patch -p1 < "$1"
}

patch_config_sub() {
    SRC="$1/tools/pkg/config.sub"
    find "." -type f -name "config.sub" -exec sudo cp "$SRC" {} \;
    find "." -type f -name "config.guess" -exec sudo cp "$1/tools/pkg/config.guess" {} \;
}

fast_install() {
    old0="$(pwd)"
    wget "$2"
    archive_name="$(basename $2)"
    dir_name=$(tar -tf "$archive_name" | head -1 | cut -f1 -d"/")
    tar -xvf "$archive_name"
    cd "$dir_name"
    autotools_recursive_regen
    if [ -n "$4" ]; then
        diff_patch "$4"
    fi
    ./configure --prefix="/usr" --host=x86_64-orange-mlibc --disable-static --enable-shared --disable-malloc0returnsnull $3
    make -j$(nproc)
    make install DESTDIR="$1"

    old="$(pwd)"
    cd "$1/usr/lib"
    rm -rf *.la
    cd "$old"

    cd "$old0"
}

fast_install_debug() {
    old0="$(pwd)"
    wget "$2"
    archive_name="$(basename $2)"
    dir_name=$(tar -tf "$archive_name" | head -1 | cut -f1 -d"/")
    #tar -xvf "$archive_name"
    cd "$dir_name"
    #autotools_recursive_regen
    # if [ -n "$4" ]; then
    #     diff_patch "$4"
    # fi
    ./configure --prefix="/usr" --host=x86_64-orange-mlibc --disable-static --enable-shared --disable-malloc0returnsnull $3
    make -j$(nproc)
    sudo make install DESTDIR="$1"

    old="$(pwd)"
    cd "$1/usr/lib"
    rm -rf *.la
    cd "$old"

    cd "$old0"
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

base_dir="$(realpath ../../../../)"

autotools_recursive_regen() {
    for f in $(grep -rl 'GNU config.sub ($timestamp)'); do
        sudo mv "$f" "$f".reference
        sudo cp -v ${base_dir}/tools/pkg/config.sub "$f"
        sudo touch -r "$f".reference "$f"
        sudo rm -f "$f".reference
    done
    for f in $(grep -rl 'GNU config.guess ($timestamp)'); do
        sudo mv "$f" "$f".reference
        sudo cp -v ${base_dir}/tools/pkg/config.guess "$f"
        sudo touch -r "$f".reference "$f"
        sudo rm -f "$f".reference
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

#

generate_shared() {
    echo Generating shared from $2 to $1
    x86_64-orange-mlibc-gcc -shared -o "$1" -Wl,--whole-archive "$2" -Wl,--no-whole-archive -fPIC
    rm -f "$2"
}

kill_libtool_demons() {
    echo Killing libtool demons
    old="$(pwd)"
    cd "$1/usr/lib"
    rm -rf *.la
    cd "$old"
}