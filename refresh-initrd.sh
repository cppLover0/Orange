
echo ''

chmod +x initrd/etc/

mkdir -p tools/base/boot

cp -rf tools/fastfetch initrd/root
tar -cf tools/base/boot/initrd.tar -C initrd .
