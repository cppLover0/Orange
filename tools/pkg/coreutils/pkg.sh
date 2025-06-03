
. ../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

install_gnu coreutils coreutils 9.7
mkdir -p coreutils-build

cd coreutils-9.7
diff_patch ../../diff/coreutils.diff
cd ..

cd coreutils-build
../coreutils-9.7/configure --host=x86_64-orange --prefix="$1" CFLAGS="-std=gnu17" 
make install -j$(nproc)

clear_share "$1"

cd ..