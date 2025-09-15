
mkdir -p "$1/etc/drivers/"

x86_64-orange-gcc -o "$1/etc/drivers/socket_test.sys" src/server.c
x86_64-orange-gcc -o "$1/usr/bin/socket_test" src/client.c