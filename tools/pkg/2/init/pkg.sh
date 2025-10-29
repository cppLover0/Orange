
rm -rf pack
mkdir -p pack/lib

cd pack/lib/
git clone https://codeberg.org/Mintsuki/Flanterm.git
cd ../../

x86_64-orange-mlibc-g++ -c src/main.cpp -o "pack/init.o" -Ipack/lib/Flanterm/src -Isrc/include 
x86_64-orange-mlibc-gcc -c pack/lib/Flanterm/src/flanterm.c -o pack/flanterm.o -Ipack/lib/Flanterm/src
x86_64-orange-mlibc-gcc -c pack/lib/Flanterm/src/flanterm_backends/fb.c -o pack/flanterm_backends.o -Ipack/lib/Flanterm/src 

x86_64-orange-mlibc-g++ -o "$1/usr/bin/init" pack/init.o pack/flanterm.o pack/flanterm_backends.o 
