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

#fuck xv

# wget https://xv.trilon.com/dist/xv-3.10a.tar.gz
# tar -xvf xv-3.10a.tar.gz
# cd xv-3.10a

# diff_patch ../../diff/xv.diff

# CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-g++ CPP=x86_64-orange-mlibc-g++ LD=x86_64-orange-mlibc-ld PKGCONFIG=x86_64-orange-mlibc-pkg-config PKG_CONFIG=x86_64-orange-mlibc-pkg-config make -j$(nproc) CFLAGS="-Wno-implicit-function-declaration -fPIC -DHAVE_STDLIB_H -Wno-implicit-int -Wno-implicit-function-declaration"
# echo installing xv
# CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-g++ CPP=x86_64-orange-mlibc-g++ LD=x86_64-orange-mlibc-ld PKGCONFIG=x86_64-orange-mlibc-pkg-config PKG_CONFIG=x86_64-orange-mlibc-pkg-config make install DESTDIR="$(realpath build)" 

# cd ..

# wget http://deb.debian.org/debian/pool/main/x/xloadimage/xloadimage_4.1.orig.tar.gz
# tar -xvf xloadimage_4.1.orig.tar.gz

# cd xloadimage.4.1

# autotools_recursive_regen
# diff_patch ../../diff/xloadimage.diff

# CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-g++ CPP=x86_64-orange-mlibc-g++ LD=x86_64-orange-mlibc-ld PKGCONFIG=x86_64-orange-mlibc-pkg-config PKG_CONFIG=x86_64-orange-mlibc-pkg-config make -j$(nproc)
# CC=x86_64-orange-mlibc-gcc CXX=x86_64-orange-mlibc-g++ CPP=x86_64-orange-mlibc-g++ LD=x86_64-orange-mlibc-ld PKGCONFIG=x86_64-orange-mlibc-pkg-config PKG_CONFIG=x86_64-orange-mlibc-pkg-config make install DESTDIR="$1" SYSPATHFILE="$1/usr/lib/X11/Xloadimage" INSTALLDIR="$1/usr/bin"


# cd ..

git clone https://github.com/stoeckmann/xwallpaper.git

cd xwallpaper

./autogen.sh
autotools_recursive_regen
./configure --host=x86_64-orange-mlibc --prefix="/usr"
make -j$(nproc)
make install DESTDIR="$1"

cd ..