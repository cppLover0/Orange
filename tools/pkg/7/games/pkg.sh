. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

wget https://downloads.sourceforge.net/libpng/libpng-1.6.37.tar.gz
tar -xvf libpng-1.6.37.tar.gz
cd libpng-1.6.37

autotools_recursive_regen

./configure --host=x86_64-orange-mlibc --prefix=/usr --disable-shared --enable-static 
make -j$(nproc)
make install DESTDIR="$1"

cd ..

export CFLAGS="-Wno-incompatible-pointer-types -Wno-error -fPIC -Wno-implicit-function-declaration"

fast_install "$1" https://www.delorie.com/store/ace/ace-1.4.tar.gz "--enable-shared" "../../diff/ace.diff" 

cd ../