cp -rf tools/initbase/* initrd

mkdir -p tools/base/boot
tar -cf tools/base/boot/initrd.tar -C initrd .
