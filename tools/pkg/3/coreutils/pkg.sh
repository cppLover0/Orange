. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

installgnu coreutils coreutils 9.7
mkdir -p coreutils-build

cd coreutils-9.7
diff_patch ../../diff/coreutils.diff
patch_config_sub "$(realpath $1/..)"
cd ..

cd coreutils-build
../coreutils-9.7/configure --host=x86_64-orange-mlibc --prefix="/usr" 
make install-strip -j$(nproc) DESTDIR="$1"

cd ..