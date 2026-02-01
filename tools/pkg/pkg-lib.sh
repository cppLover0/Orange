
GNU_MIRROR=https://mirror.dogado.de/
CFLAGS="$CFLAGS -lstdc++"

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
    tar -xf "$archive_name"
    cd "$dir_name"
    autotools_recursive_regen
    if [ -n "$4" ]; then
        diff_patch "$4"
    fi
    ./configure --prefix="/usr" --disable-static --host=x86_64-linux-gnu --enable-shared --disable-malloc0returnsnull --bindir=/usr/bin --libdir=/usr/lib --sbindir=/usr/bin --sysconfdir=/etc --localstatedir=/var --with-sysroot="$1" $3
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
    :
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
