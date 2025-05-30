
. ../pkg_lib.sh

rm -rf pack
mkdir -p pack

cd pack

wget https://www.lua.org/ftp/lua-5.4.7.tar.gz

tar -xvf lua-5.4.7.tar.gz
cd lua-5.4.7

make CC=x86_64-orange-gcc LD=x86_64-orange-ld generic -j$(nproc)
cp -rf src/lua "$1/bin"
cp -rf src/luac "$1/bin"

cd ..
