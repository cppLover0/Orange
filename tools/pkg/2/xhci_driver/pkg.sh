
mkdir -p "$1/etc/drivers/"

x86_64-orange-g++ -o "$1/etc/drivers/xhcidriver.wsys" src/main.cpp -Iinclude -std=c++17