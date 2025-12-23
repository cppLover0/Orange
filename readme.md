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

- [x] hpet
- [x] pvclock
- [x] ioapic
- [x] ps/2 
- [x] ps/2 keyboard
- [x] ps/2 mouse
- [x] xhci
- [x] usb keyboard
- [x] usb mouse

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

Dependencies
```
cmake meson (1.4.2) ninja gcc (13.3.0) libgmp3-dev libmpfr-dev libmpc-dev libglib2.0-dev-bin python3.13 python3.13-dev python3.13-venv python3.13-distutils gettext gperf rsync flex bison make nasm bash 
```

Host python dependencies
```
mako pyyaml
```

Also if you have custom python version you need to pass environment variable because python requires newest version
```
BUILDPYTHON=--with-build-python=python3.13 sh tar-initrd.sh
```

## TODO

- [x] Move XHCI driver from old kernel to userspace
- [x] Implement IRQ userspace handling
- [x] Port lua, fastfetch, doomgeneric, nano and etc.
- [x] Improve XHCI driver
- [x] Port Xorg 
- [ ] Port twm 
- [ ] Port wine
- [ ] Implement userspace disk drivers
- [x] Improve kernel path resolver (add symlink support when trying to access another filesystem)