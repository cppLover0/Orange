
export PATH="$HOME/opt/cross/orange/bin:$PATH"

if [ ! "$(which x86_64-orange-gcc)" ]; then
    echo "It looks like you don't have the cross-compiler installed, or it isn't in your PATH."
    echo "If you built your cross-compiler, add it to your PATH with:"
    echo 'export PATH="$HOME/opt/cross/orange/bin:$PATH"'
    echo 'Alternatively, you can build the cross-compiler with: "sh build-cross.sh"'
    echo 'Also you should have host gcc with version < 14 (i am using 13.3.0)'
    exit 1
fi

INITRDDIR=""$(realpath initrd)""

cd tools/pkg/$1 
sh pkg.sh "$INITRDDIR"
cd ../../../../

cd initrd/lib

echo Creating symlinks ./*.so
for file in ../usr/lib/*.so; do
	echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

for file in ../usr/local/lib/*.so; do
	echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

cd ../bin

echo Creating symlinks ./*
for file in ../usr/bin/*; do
	echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

for file in ../usr/local/bin/*; do
	echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

cd ../../

mkdir -p tools/base/boot
tar -cf tools/base/boot/initrd.tar -C initrd .
