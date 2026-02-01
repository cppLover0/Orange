
echo ''

chmod +x initrd/etc/

mkdir -p tools/base/boot
tar -cf tools/base/boot/initrd.tar -C initrd .
