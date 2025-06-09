# Nuke built-in rules and variables.
MAKEFLAGS += -rR
.SUFFIXES:

# Default user QEMU flags. These are appended to the QEMU command calls. 
QEMUFLAGS := -m 1G -no-reboot -serial stdio -M q35 -s -d int -smp 2 -trace usb_xhci_* -trace "msi*" -device qemu-xhci -device usb-kbd -enable-kvm
override IMAGE_NAME := orange

# Toolchain for building the 'limine' executable for the host.
HOST_CC := cc
HOST_CFLAGS := -g -pipe 
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=

HOST_LD := ld
HOST_TAR := tar

CURRENT_DIR := $(shell realpath .)

ifeq ($(shell uname), Darwin)
    HOST_LD = ld.lld
	HOST_TAR = gtar
endif

.PHONY: run-build
run-build:all run

.PHONY: uefi-run-build
uefi-run-build: all run-uefi

.PHONY: headers
headers:
	rm -rf orange-cross-compiler-headers
	git clone https://github.com/cpplover0/orange-cross-compiler-headers
	mkdir -p initrd/usr/include
	rm -rf initrd/usr/include/*
	mkdir -p orange-cross-compiler-headers/include
	cp -rf orange-cross-compiler-headers/include initrd/usr
	rm -rf orange-cross-compiler-headers

.PHONY: make-libc
make-libc:
	rm -rf tools/orange-mlibc
	cd tools && git clone https://github.com/cpplover0/orange-mlibc --depth=1
	cd tools/orange-mlibc && sh build_to_cross.sh "$(CURRENT_DIR)"

.PHONY: cross-compiler
cross-compiler: make-libc
	cd tools/toolchain/ && sh get.sh "$(CURRENT_DIR)"

.PHONY: check-cross
check-cross:
    ifeq (, $(shell which x86_64-orange-gcc))
		echo "It looks like you don't have cross-compiler, or you didn't add them to your PATH."; \
		echo "If you built your cross compiler, add them to PATH with: "; \
		echo 'export PATH="$$HOME/opt/cross/orange/bin:$$PATH"'; \
		echo 'or build cross compiler with "make cross-compiler"'; \
		exit 1; 
    endif

.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: all-hdd
all-hdd: $(IMAGE_NAME).hdd

.PHONY: run
run: $(IMAGE_NAME).iso
	qemu-system-x86_64 \
		-M q35 \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		$(QEMUFLAGS)

.PHONY: run-uefi
run-uefi: ovmf/ovmf-code-x86_64.fd $(IMAGE_NAME).iso
	qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-x86_64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		$(QEMUFLAGS)

.PHONY: run-hdd
run-hdd: $(IMAGE_NAME).hdd
	qemu-system-x86_64 \
		-M q35 \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

.PHONY: run-hdd-uefi
run-hdd-uefi: ovmf/ovmf-code-x86_64.fd $(IMAGE_NAME).hdd
	qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-x86_64.fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

ovmf/ovmf-code-x86_64.fd:
	mkdir -p ovmf
	curl -Lo $@ https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/download/ovmf-code-x86_64.fd

limine/limine:
	rm -rf limine
	git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1
	$(MAKE) -C limine \
		CC="$(HOST_CC)" \
		CFLAGS="$(HOST_CFLAGS)" \
		CPPFLAGS="$(HOST_CPPFLAGS)" \
		LDFLAGS="$(HOST_LDFLAGS)" \
		LIBS="$(HOST_LIBS)"

kernel-deps:
	./kernel/get-deps
	touch kernel-deps

.PHONY: kernel
kernel: kernel-deps
	rm -rf kernel/src/lib/uACPI/tests
	$(MAKE) -C kernel

$(IMAGE_NAME).iso: limine/limine kernel
	rm -rf iso_root
	mkdir -p iso_root/boot
	cp -v kernel/bin/kernel iso_root/boot/
	mkdir -p iso_root/boot/limine
	cp -v tools/limine.conf limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp -rf tools/iso/* iso_root/
	mkdir -p iso_root/EFI/BOOT
	cp -v limine/BOOTX64.EFI iso_root/EFI/BOOT/
	cp -v limine/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./limine/limine bios-install $(IMAGE_NAME).iso
	rm -rf iso_root

$(IMAGE_NAME).hdd: limine/limine kernel
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00
	./limine/limine bios-install $(IMAGE_NAME).hdd
	mformat -i $(IMAGE_NAME).hdd@@1M
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel/bin/kernel ::/boot
	mcopy -i $(IMAGE_NAME).hdd@@1M limine.conf limine/limine-bios.sys ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTIA32.EFI ::/EFI/BOOT

.PHONY: clean
clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root $(IMAGE_NAME).iso $(IMAGE_NAME).hdd

.PHONY: distclean
distclean: clean
	$(MAKE) -C kernel distclean
	rm -rf kernel-deps limine ovmf
