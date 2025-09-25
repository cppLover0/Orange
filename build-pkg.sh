
export PATH="$HOME/opt/cross/orange/bin:$PATH"

INITRDDIR=""$(realpath initrd)""

cd tools/pkg/$1 
sh pkg.sh "$INITRDDIR"
cd ../../../../

cd initrd/lib

#echo Creating symlinks ./*.so
for file in ../usr/lib/*.so; do
	#echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

for file in ../usr/local/lib/*.so; do
	#echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

cd ../bin

#echo Creating symlinks ./*
for file in ../usr/bin/*; do
	#echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

for file in ../usr/local/bin/*; do
	#echo $file "$(basename "$file")"
    ln -sf "$file" "$(basename "$file")"
done

cd ../../

mkdir -p tools/base/boot
tar -cf tools/base/boot/initrd.tar -C initrd .
