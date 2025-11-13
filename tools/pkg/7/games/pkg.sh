. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

export CFLAGS="-Wno-incompatible-pointer-types -Wno-error -fPIC -Wno-implicit-function-declaration"

fast_install "$1" https://www.delorie.com/store/ace/ace-1.4.tar.gz "--enable-shared" "../../diff/ace.diff" 

cd ../