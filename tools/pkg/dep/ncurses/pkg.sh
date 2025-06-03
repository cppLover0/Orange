


. ../../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

install_gnu ncurses ncurses 6.5
mkdir -p ncurses-build

cd ncurses-6.5
diff_patch ../../diff/ncurses.diff
cd ..

cd ncurses-build
../ncurses-6.5/configure --host=x86_64-orange CFLAGS="-std=gnu17" --prefix=/usr --with-shared --without-ada
make -j$(nproc)

path1="$(realpath $1/..)"
make install -j$(nproc) DESTDIR="$path1"

rm -rf "$1/lib/*.a"

cz=$(pwd)

cd "$1/lib"
ln -s libncursesw.so libcurses.so
ln -s libncursesw.so libcursesw.so

cd "$cz"

cd "$1/include/"

for file in "ncurses/*"; do
	echo $file "$(basename "$file")"
    ln -s "$file" "$(basename "$file")"
done

cd "$cz"

cd ../..