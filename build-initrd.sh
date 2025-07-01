
CURRENT_DIR="$(realpath .)"

export PATH="$HOME/opt/cross/orange/bin:$PATH"

if [ ! "$(which x86_64-orange-gcc)" ]; then
    echo "It looks like you don't have the cross-compiler installed, or it isn't in your PATH."
    echo "If you built your cross-compiler, add it to your PATH with:"
    echo 'export PATH="$HOME/opt/cross/orange/bin:$PATH"'
    echo 'Alternatively, you can build the cross-compiler with: "make cross-compiler"'
    echo 'Also you should have host gcc with version < 14 (i am using 13.3.0)'
    exit 1
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

for file in ../usr/local/lib/*.so; do
	echo $file "$(basename "$file")"
    ln -s "$file" "$(basename "$file")"
done

cd ../bin

echo Creating symlinks ./*
for file in ../usr/bin/*; do
	echo $file "$(basename "$file")"
    ln -s "$file" "$(basename "$file")"
done

for file in ../usr/local/bin/*; do
	echo $file "$(basename "$file")"
    ln -s "$file" "$(basename "$file")"
done


cd ../../

cp -rf tools/base/* initrd/

echo Building initrd is done

mkdir -p tools/iso/boot
tar -cf tools/iso/boot/initrd.tar -C initrd .

