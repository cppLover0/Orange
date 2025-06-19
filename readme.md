
# Orange

Orange is my unix-like x86_64 OS
Currenly it have bash,coreutils,lua and fastfetch ports

[![GitHub top language](https://img.shields.io/github/languages/top/cpplover0/orange?logo=c&label=)](https://github.com/cpplover0/orange/blob/main/kernel/GNUmakefile)
[![GitHub license](https://img.shields.io/github/license/cpplover0/orange)](https://github.com/cpplover0/orange/blob/master/LICENSE)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/e78ad48f394f46d1bb98f1942c7e1f21)]()
[![GitHub contributors](https://img.shields.io/github/contributors/cpplover0/orange)](https://github.com/cpplover0/orange/graphs/contributors)
[![GitHub commit activity](https://img.shields.io/github/commit-activity/m/cpplover0/orange)](https://github.com/cpplover0/orange/commits)

## Preview
![fastfetch and lua](https://github.com/cppLover0/Orange/blob/main/tools/img/work.png?raw=true)

## Devices which supported by orange

- [x] Serial
- [x] CMOS
- [x] HPET
- [x] IOAPIC
- [x] PS/2 Keyboard
- [x] XHCI
- [x] USB Keyboard

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

- [x] bash
- [x] coreutils
- [ ] doom2
- [ ] nasm
- [ ] gcc
- [x] lua
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
- [x] XHCI/HID/USB
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