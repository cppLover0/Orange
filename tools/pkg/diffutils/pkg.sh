
. ../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

rm -rf "$1/bin/diff"

install_gnu diffutils diffutils 3.12
mkdir -p diffutils-build

cd diffutils-3.12
diff_patch ../../diff/diffutils.diff
cd ..

cd diffutils-build
../diffutils-3.12/configure --host=x86_64-orange CFLAGS="-std=gnu17" --prefix="$(realpath $1)" gl_cv_func_strcasecmp_works=yes 
make install -j$(nproc)

cd ..