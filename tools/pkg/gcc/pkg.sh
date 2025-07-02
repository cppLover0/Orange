
. ../pkg_lib.sh

current="$(pwd)"

rm -rf pack

mkdir -p pack

cd pack

echo $1

wget -nc $GNU_MIRROR/gnu/gcc/gcc-15.1.0/gcc-15.1.0.tar.gz
tar -xvf gcc-15.1.0.tar.gz

cp -rf gcc-15.1.0 gcc-build

cd gcc-build

diff_patch "$current/diff/gcc.diff"

echo "$current/diff/gcc.diff"

./configure --host=x86_64-orange --with-sysroot="/" --prefix="/usr" --with-build-sysroot="$(realpath $1/..)" --enable-languages=c --disable-bootstrap --enable-default-pie --enable-default-ssp --disable-multilib --disable-lto --disable-libssp --disable-threads --disable-libatomic --disable-libffi --disable-libgomp --disable-libitm --disable-libsanitizer --disable-bootstrap --enable-shared --enable-host-shared CFLAGS="-Os"

make clean
make -j$(nproc)
DESTDIR="$(realpath $1/..)" make install-strip -j$(nproc)

cp -rf host-x86_64-orange/gcc/specs "$1/lib/gcc/x86_64-orange/15.1.0/specs"
cp -rf host-x86_64-orange/gcc/specs "$1/lib/gcc/x86_64-orange/specs"
cd ..