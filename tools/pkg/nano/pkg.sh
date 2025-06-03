

. ../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

install_gnu nano nano 8.4
mkdir -p nano-build

cd nano-8.4
diff_patch ../../diff/nano.diff
cd ..

cd nano-build
../nano-8.4/configure --host=x86_64-orange CFLAGS="-std=gnu17" --prefix=$1 gl_cv_func_strcasecmp_works=yes 

make -j$(nproc)
make install -j$(nproc)

cd ../