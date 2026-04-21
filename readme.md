# Orange

Orange is my linux-like x86_64, aarch64, riscv64 os

[![GitHub top language](https://img.shields.io/github/languages/top/cpplover0/orange?logo=c&label=)](https://github.com/cpplover0/orange/blob/main/kernel/GNUmakefile)
[![GitHub license](https://img.shields.io/github/license/cpplover0/orange)](https://github.com/cpplover0/orange/blob/master/LICENSE)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/e78ad48f394f46d1bb98f1942c7e1f21)]()
[![GitHub contributors](https://img.shields.io/github/contributors/cpplover0/orange)](https://github.com/cpplover0/orange/graphs/contributors)
[![GitHub commit activity](https://img.shields.io/github/commit-activity/m/cpplover0/orange)](https://github.com/cpplover0/orange/commits)

for build do
```sh
make run -j$(nproc) ARCH=$ARCH TOOLCHAIN=llvm
```

for building distro you can use python script
```sh
python3 orangestrap.py orange_sysroot build distro
```

to pack initrd 
```sh
sh create-initrd.sh
```

requirements:
```
rsync, meson, ninja, gcc 15 with g++, cmake, make, git, python3, coreutils, sh (bash), llvm, clang (for kernel, im tesing with clang 20 also clang++), tar, qemu, xorriso, texinfo, bison, flex, autoconf, automake, libtool
```

there's also cmdline for some stuff: init=path, notsc, noacpi
noacpi means only table init