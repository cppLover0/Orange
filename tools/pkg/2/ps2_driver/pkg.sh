
mkdir -p "$1/etc/drivers/"

x86_64-orange-mlibc-gcc -o "$1/etc/drivers/ps2driver.sys" src/main.c