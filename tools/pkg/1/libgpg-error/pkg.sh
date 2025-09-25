. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

wget https://www.gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-1.55.tar.bz2
tar -xvf libgpg-error-1.55.tar.bz2

cd libgpg-error-1.55
diff_patch ../../diff/libgpg-error.diff

./configure --prefix=/usr --host=x86_64-orange  --enable-threads --enable-install-gpg-error-config

make install -j$(nproc) DESTDIR="$1"

cd ..

