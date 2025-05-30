
CURRENT_DIR="$(realpath .)"

export PATH="$HOME/opt/cross/orange/bin:$PATH"

if [!$(which x86_64-orange-gcc)] 
then
		echo "It looks like you don't have cross-compiler, or you didn't add them to your PATH."; \
		echo "If you built your cross compiler, add them to PATH with: "; \
		echo 'export PATH="$$HOME/opt/cross/orange/bin:$$PATH"'; \
		echo 'or build cross compiler with "make cross-compiler"'; \
fi

rm -rf tools/orange-mlibc
rm -rf initrd
mkdir -p initrd/lib
cd tools
sh get-linux-headers.sh "$CURRENT_DIR"
git clone https://github.com/cpplover0/orange-mlibc --depth=1
cd orange-mlibc 
bash build_to_cross.sh "$CURRENT_DIR"
cd ../../
rm -rf initrd/lib/*
cd initrd/lib
ln -s ../usr/lib/ld.so ld64.so.1 
cd ../../
mkdir -p initrd/bin
mkdir -p initrd/usr/bin
x86_64-orange-gcc tools/test_init/main.c -o initrd/usr/bin/initrd -no-pie
rm -rf tools/iso/boot/initrd.tar
cd tools/pkg
bash build_pkg.sh "$CURRENT_DIR/initrd/usr"
cd ../../

mkdir -p initrd/lib
mkdir -p initrd/bin

cd initrd/lib

echo Creating symlinks ./*.so
for file in ../usr/lib/*.so; do
	echo $file "$(basename "$file")"
    ln -s "$file" "$(basename "$file")"
done

cd ../bin

echo Creating symlinks ./*.so
for file in ../usr/bin/*; do
	echo $file "$(basename "$file")"
    ln -s "$file" "$(basename "$file")"
done

cd ../../

cp -rf tools/base/* initrd/

echo Building initrd is done

tar -cf tools/iso/boot/initrd.tar -C initrd .

