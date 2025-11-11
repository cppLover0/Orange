
mkdir -p "$1/etc/drivers/"

x86_64-orange-mlibc-gcc -o "$1/etc/drivers/i8042controller.sys" src/main.c