rm -rf orange_sysroot/dev orange_sysroot/proc orange_sysroot/sys
mkdir -p orange_sysroot/dev orange_sysroot/proc orange_sysroot/sys
tar --format=ustar -cf baseiso/boot/initrd.ustar -C orange_sysroot .
