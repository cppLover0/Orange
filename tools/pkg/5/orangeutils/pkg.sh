x86_64-orange-mlibc-gcc src/orangedebugenable.c -o "$1/usr/bin/orangedebugenable" -Wno-implicit-function-declaration
x86_64-orange-mlibc-gcc src/orangeprintinfo.c -o "$1/usr/bin/orangeprintinfo" -Wno-implicit-function-declaration
x86_64-orange-mlibc-gcc src/orangedebug.c -o "$1/usr/bin/orangedebug" -Wno-implicit-function-declaration
x86_64-orange-mlibc-gcc src/dmesg.c -o "$1/usr/bin/dmesg" -Wno-implicit-function-declaration