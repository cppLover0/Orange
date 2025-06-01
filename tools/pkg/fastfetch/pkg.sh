
. ../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

wget https://github.com/fastfetch-cli/fastfetch/archive/refs/tags/2.44.0.tar.gz

tar -xvf 2.44.0.tar.gz

cd fastfetch-2.44.0
diff_patch ../../diff/fastfetch.diff
cd ..

mkdir -p fastfetch-build
cd fastfetch-build
cmake ../fastfetch-2.44.0 -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../../toolchain.cmake) -DCMAKE_INSTALL_PREFIX=$1

make -j$(nproc)
make install 

cd ..