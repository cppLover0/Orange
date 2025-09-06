# Orange

Orange is my posix x86_64 os with microkernel features

[![GitHub top language](https://img.shields.io/github/languages/top/cpplover0/orange?logo=c&label=)](https://github.com/cpplover0/orange/blob/main/kernel/GNUmakefile)
[![GitHub license](https://img.shields.io/github/license/cpplover0/orange)](https://github.com/cpplover0/orange/blob/master/LICENSE)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/e78ad48f394f46d1bb98f1942c7e1f21)]()
[![GitHub contributors](https://img.shields.io/github/contributors/cpplover0/orange)](https://github.com/cpplover0/orange/graphs/contributors)
[![GitHub commit activity](https://img.shields.io/github/commit-activity/m/cpplover0/orange)](https://github.com/cpplover0/orange/commits)

## Preview
![fastfetch and lua](https://github.com/cppLover0/Orange/blob/main/tools/img/work.png?raw=true)

## Devices which supported by orange

- [x] serial
- [x] cmos
- [x] hpet
- [x] pvclock
- [x] ioapic
- [x] ps/2 Keyboard

# Build

Build kernel
```sh
make all
```

Build initrd (You should build it before running os)
```sh
sh tar-initrd.sh
```

Build cross-compiler 
```sh
sh build-cross.sh
```

Build kernel and run iso 
```sh
make run
```

## TODO

- [ ] Move XHCI driver from old kernel to userspace
- [x] Implement IRQ userspace handling
- [ ] Port lua, fastfetch, doomgeneric, nano and etc.
- [ ] Port Xorg 
- [ ] Implement userspace disk drivers
- [ ] Improve kernel path resolver (add symlink support when trying to access another filesystem)