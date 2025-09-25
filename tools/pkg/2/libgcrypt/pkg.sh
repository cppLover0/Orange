. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

wget https://www.gnupg.org/ftp/gcrypt/libgcrypt/libgcrypt-1.11.2.tar.bz2
tar -xvf libgcrypt-1.11.2.tar.bz2

cd libgcrypt-1.11.2

diff_patch ../../diff/libgcrypt.diff

./configure --prefix=/usr --host=x86_64-orange --with-libgpg-error-prefix="$1/usr" 

make -j$(nproc)
make install -j$(nproc) DESTDIR="$1"

cd ..

