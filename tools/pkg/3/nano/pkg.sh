. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

installgnu nano nano 8.4
mkdir -p nano-build

cd nano-8.4
diff_patch ../../diff/nano.diff
patch_config_sub "$(realpath $1/..)"
cd ..

cd nano-build
../nano-8.4/configure --host=x86_64-linux-gnu --prefix="/usr" gl_cv_func_strcasecmp_works=yes CFLAGS="-std=gnu17"
make install -j$(nproc) DESTDIR="$1"

cd ..