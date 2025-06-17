
. ../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

install_gnu findutils findutils 4.6.0
mkdir -p findutils-build

cd findutils-4.6.0
diff_patch ../../diff/findutils.diff
cd ..

cd findutils-build
../findutils-4.6.0/configure --host=x86_64-orange --prefix="$1" CFLAGS="-std=gnu17" 
make install -j$(nproc)

cd ..