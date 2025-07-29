
mkdir -p "$1/usr/include/orange"

x86_64-orange-gcc -shared -fPIC -o "$1/usr/lib/liborange.so" src/lib.c