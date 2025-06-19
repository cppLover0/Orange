
. ../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

install_gnu grep grep 3.12
mkdir -p grep-build

cd grep-3.12
diff_patch ../../diff/grep.diff
cd ..

cd grep-build
../grep-3.12/configure --host=x86_64-orange --prefix="$1" CFLAGS="-std=gnu17" 
make install -j$(nproc)

cd ..