
# my friend asked so why not...
. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack
cd pack

git clone https://github.com/luau-lang/luau.git --depth=1
cd luau

mkdir -p luau-build
cd luau-build

export CFLAGS="$CFLAGS -D_XOPEN_SOURCE=700"
export CXXFLAGS="$CXXFLAGS -D_XOPEN_SOURCE=700"

cmake .. -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../../../../toolchain.cmake) -DCMAKE_INSTALL_PREFIX="$1/usr"

make -j$(nproc)
cp -rf luau* "$1/usr/bin"
cp -rf libLuau* libisocline.a "$1/usr/lib"
cd ..

cd ..