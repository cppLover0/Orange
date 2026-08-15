. "${pkg_lib}"

export CROSS_COMPILE='1'

prepare() {
    autotools_recursive_regen

    cd nspr
    autoreconf -fvi
}

configure() {
    cp -rf "${source_dir}"/* .

    cd nspr
    ./configure --prefix=/usr --host=x86_64-linux
}

build() {
    cd nspr

    echo building config
    cd config
    make CC=gcc CXX=g++
    mv nsinstall nsinstall-host

    sed -s 's#/nsinstall$#/nsinstall-host#' -i autoconf.mk
    rm -rf nsinstall.o

    cd ../pr/include

    sed -i 's/EXPORTS\s*=\s*\$(HEADERS)/EXPORTS = $(addprefix $(srcdir)\/, $(HEADERS))/' Makefile
    sed -i 's/ReleaseHdrDir\s*=\s*\$(RELEASE_INCLUDE_DIR)/ReleaseHdrDir = $(RELEASE_INCLUDE_DIR)/' Makefile

    echo building nspr
    cd ../..

    rm -rf dist/include/nspr/.

    make CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-g++ -j1
}

install() {
    cd nspr
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit