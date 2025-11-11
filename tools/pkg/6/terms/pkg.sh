. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack

cd pack

git clone https://github.com/Shourai/st.git
cd st

mkdir build

CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-g++ CPP=x86_64-orange-mlibc-g++ LD=x86_64-orange-mlibc-ld PKGCONFIG=x86_64-orange-mlibc-pkg-config PKG_CONFIG=x86_64-orange-mlibc-pkg-config make -j$(nproc) CFLAGS="-Wno-implicit-function-declaration -fPIC"
CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-g++ CPP=x86_64-orange-mlibc-g++ LD=x86_64-orange-mlibc-ld PKGCONFIG=x86_64-orange-mlibc-pkg-config PKG_CONFIG=x86_64-orange-mlibc-pkg-config make install DESTDIR="$(realpath build)"

cp -rf build/usr/local/* "$1/usr/"

cd ..

fast_install "$1" https://invisible-mirror.net/archives/xterm/xterm-390.tgz "--disable-tcap-fkeys --disable-tcap-query --enable-256-color" "../../diff/xterm.diff"
fast_install "$1" https://www.x.org/archive/individual/lib/libXrandr-1.5.3.tar.gz
fast_install "$1" https://www.x.org/archive/individual/app/xev-1.2.5.tar.gz