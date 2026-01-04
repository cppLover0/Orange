
mkdir -p "$1/etc/drivers"

cp -rf dbus-start-all.sh "$1/usr/bin/dbus-start-all.sh"
chmod +x "$1/usr/bin/dbus-start-all.sh"
x86_64-orange-mlibc-gcc -o "$1/etc/drivers/dbus.sys" main.c