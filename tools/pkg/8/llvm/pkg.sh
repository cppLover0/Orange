
. ../../pkg-lib.sh

exit 0 #im not compiling this shit 

rm -rf pack

mkdir -p pack

cd pack

wget https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.5/llvm-project-21.1.5.src.tar.xz
tar -xvf llvm-project-21.1.5.src.tar.xz
cd llvm-project-21.1.5.src

diff_patch ../../diff/llvm.diff
mkdir build

cd build
cmake ../llvm -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-D_GNU_SOURCE=1 -fPIC -O3" -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE -DCMAKE_CXX_STANDARD=17 -DUNIX=1 -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../../../../toolchain.cmake) -DCMAKE_INSTALL_PREFIX="/usr" -UBUILD_SHARED_LIBS -UENABLE_STATIC  -DLLVM_LINK_LLVM_DYLIB=ON -DLLVM_ENABLE_FFI=ON -DLLVM_ENABLE_EH=ON -DLLVM_ENABLE_RTTI=ON -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-orange-mlibc -DLLVM_HOST_TRIPLE=x86_64-orange-mlibc -Wno-dev

mkdir temp_sysroot

cmake --build . -j$(nproc)
DESTDIR="$(realpath temp_sysroot)" cmake --install .

rm -rf temp_sysroot/bin
rm -rf temp_sysroot/lib/*.a

cp -r temp_sysroot "$1"

cd ../..