. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

installgnu coreutils coreutils 9.8
mkdir -p coreutils-build

cd coreutils-build
../coreutils-9.8/configure --host=x86_64-linux-gnu --prefix="/usr" 
make install-strip -j$(nproc) DESTDIR="$1"

cd ..
