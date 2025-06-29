
. ../../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

wget https://zlib.net/zlib-1.3.1.tar.gz
tar -xvf zlib-1.3.1.tar.gz

cd zlib-1.3.1
diff_patch ../../diff/zlib.diff
cd ..

mkdir zlib-build
cd zlib-build

CC=x86_64-orange-gcc LD=x86_64-orange-ld ../zlib-1.3.1/configure 

make -j$(nproc)
make install -j$(nproc) DESTDIR="$1" 

cd ../..