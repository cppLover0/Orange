. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

installgnu coreutils coreutils 9.8
mkdir -p coreutils-build

cd coreutils-9.8
diff_patch ../../diff/coreutils.diff
patch_config_sub "$(realpath $1/..)"
cd ..

cd coreutils-build
gl_cv_host_operating_system="orange" ../coreutils-9.8/configure --host=x86_64-orange-mlibc --prefix="/usr" --enable-no-install-program="mkfifo,runcon,chroot,mount,mountlist,tail" CFLAGS="-DSLOW_BUT_NO_HACKS -O2"
make install-strip -j$(nproc) DESTDIR="$1"

cd ..
