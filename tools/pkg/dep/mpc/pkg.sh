
. ../../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

export LD_LIBRARY_PATH="$1/lib:$LD_LIBRARY_PATH"

install_gnu mpfr mpfr 4.2.2
install_gnu mpc mpc 1.3.1
install_gnu gmp gmp 6.3.0

cd gmp-6.3.0

diff_patch ../../diff/gmp.diff

CC=x86_64-orange-gcc LD=x86_64-orange-ld ./configure --prefix=/usr --host=x86_64-orange --enable-shared

make -j$(nproc)
make install -j$(nproc) DESTDIR="$(realpath $1/..)" 

rm -rf $(find "$1" -name "*.la")

cd ..

cd mpfr-4.2.2

diff_patch ../../diff/mpfr.diff

CC=x86_64-orange-gcc LD=x86_64-orange-ld ./configure --prefix=/usr --host=x86_64-orange --enable-shared

make -j$(nproc)
make install -j$(nproc) DESTDIR="$(realpath $1/..)" 

rm -rf $(find "$1" -name "*.la")

cd ..

cd mpc-1.3.1

diff_patch ../../diff/mpc.diff

CC=x86_64-orange-gcc LD=x86_64-orange-ld ./configure --prefix=/usr --host=x86_64-orange --enable-shared=yes --enable-static=no

make -j$(nproc)
make install -j$(nproc) DESTDIR="$(realpath $1/..)" 

rm -rf $(find "$1" -name "*.la")

cd ../..