. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

fast_install "$1" $GNU_MIRROR/gnu/libiconv/libiconv-1.18.tar.gz
fast_install "$1" $GNU_MIRROR/gnu/gettext/gettext-0.26.tar.gz "--enable-shared" ../../diff/gettext.diff

cd ..
