
. ../pkg_lib.sh

current="$(pwd)"

rm -rf pack

mkdir -p pack

cd pack

echo $1

mkdir gcc-build
cd gcc-build

cp -rf ../../../../toolchain/pack/gcc-11.4.0/* .

diff_patch "$current/diff/gcc.diff"

echo "$current/diff/gcc.diff"

./configure --host=x86_64-orange --prefix="/usr" --with-sysroot="/" --with-build-sysroot="$(realpath $1/..)" --enable-languages=c --disable-nls --disable-bootstrap --enable-default-pie --enable-default-ssp --disable-multilib --disable-lto --disable-libssp --disable-threads --disable-libatomic --disable-libffi --disable-libgomp --disable-libitm --disable-libsanitizer --disable-bootstrap --enable-shared --enable-host-shared CFLAGS="-Os"

make clean
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
DESTDIR="$(realpath $1/..)" make install-gcc -j$(nproc)
DESTDIR="$(realpath $1/..)" make install-target-libgcc -j$(nproc)

cp -rf host-x86_64-orange/gcc/specs "$1/lib/gcc/x86_64-orange/11.4.0/specs"
cp -rf host-x86_64-orange/gcc/specs "$1/lib/gcc/x86_64-orange/specs"
cd ..