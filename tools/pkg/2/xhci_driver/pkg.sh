
mkdir -p "$1/etc/drivers/"

x86_64-orange-mlibc-g++ -o "$1/etc/drivers/xhcidriver.sys" src/main.cpp -Iinclude -std=c++17