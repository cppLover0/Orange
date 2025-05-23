
# Orange

Orange is my limine x86_64 OS

# Build

Build kernel
```sh
make all
```

Build initrd (You should build it before running os)
```sh
sh build-initrd.sh
```

Build kernel and run iso 
```sh
make
```


## Base x86 TODO

- [x] Build
- [x] Locking
- [x] GFX (flanterm)
- [x] PMM 
- [x] Paging
- [x] Some heap
- [x] GDT
- [x] IDT
- [x] TSS
- [x] ACPI
- [x] HPET
- [x] LAPIC 
- [x] MP
- [x] IOAPIC
- [x] Initrd support
- [x] ELF parsing
- [x] Scheduling
- [x] End of x86 pain

## Unix-like TODO

- [x] Syscalls
- [x] Start libc porting
- [ ] End libc porting

## Porting TODO

- [ ] doom2
- [ ] nasm
- [ ] gcc
- [ ] lua
- [ ] python
- [ ] etc. apps

## Driver TODO

- [x] IO
- [x] Serial
- [x] PCI
- [x] CMOS
- [x] PS2 Keyboard 
- [ ] Speaker
- [ ] AHCI
- [ ] XHCI/HID/USB
- [ ] NVME
- [ ] Networking
- [ ] Intel HDA
- [ ] End of driver pain

## Filesystem TODO

- [x] VFS
- [x] TMPFS
- [x] USTAR
- [x] DevFS
- [ ] ProcFS
- [ ] ISO9660
- [ ] FAT32
- [ ] EXT4
- [ ] End of filesystem pain

## Final TODO

- [ ] The end ?