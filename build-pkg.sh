
CURRENT_DIR="$(realpath .)"

export PATH="$HOME/opt/cross/orange/bin:$PATH"

cd tools/pkg
cd "$1"
echo Building $(cat info.txt)
bash "pkg.sh" "$CURRENT_DIR/initrd/usr"
cd ..
cd ../../

cd initrd/lib

echo Creating symlinks ./*.so
for file in ../usr/lib/*.so; do
	echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

cd ../bin

echo Creating symlinks ./*
for file in ../usr/bin/*; do
	echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

cd ../../

echo Building initrd is done

cp -rf tools/base/* initrd/
mkdir -p tools/iso/boot
tar -cf tools/iso/boot/initrd.tar -C initrd .