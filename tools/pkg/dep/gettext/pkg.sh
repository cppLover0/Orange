
. ../../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

install_gnu gettext gettext 0.25

cd gettext-0.25

diff_patch ../../diff/gettext.diff

CC=x86_64-orange-gcc LD=x86_64-orange-ld ./configure --prefix=/usr --host=x86_64-orange --enable-shared

make -j$(nproc)
make install -j$(nproc) DESTDIR="$(realpath $1/..)" 

libtool --finish "$1/lib"

cd ../..