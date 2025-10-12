
export PATH="$HOME/opt/cross/orange/bin:$PATH"

INITRDDIR=""$(realpath initrd)""

cd tools/pkg/$1 
sh pkg.sh "$INITRDDIR"
cd ../../../../

cp -rf tools/initbase/* initrd

mkdir -p tools/base/boot
tar -cf tools/base/boot/initrd.tar -C initrd .
