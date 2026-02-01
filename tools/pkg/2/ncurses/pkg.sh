. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

installgnu ncurses ncurses 6.5
mkdir -p ncurses-build

cd ncurses-build
../ncurses-6.5/configure --host=x86_64-linux-gnu --prefix="/usr" --with-shared --without-ada CFLAGS="-std=gnu17 -Wno-implicit-function-declaration"
make -j$(nproc)
make install -j$(nproc) DESTDIR="$1"

cd ..

cz=$(pwd)

cd "$1/usr/lib"
ln -sf libncursesw.so libcurses.so
ln -sf libncursesw.so libcursesw.so

ln -sf libtinfow.so libtinfo.so

cd "$cz"
