
rm -rf pack
mkdir -p pack/lib

cd pack/lib/
git clone https://github.com/mintsuki/flanterm.git
cd ../../

x86_64-orange-g++ -c src/main.cpp -o "pack/init.o" -Ipack/lib/flanterm/src -Isrc/include 
x86_64-orange-gcc -c pack/lib/flanterm/src/flanterm.c -o pack/flanterm.o -Ipack/lib/flanterm/src
x86_64-orange-gcc -c pack/lib/flanterm/src/flanterm_backends/fb.c -o pack/flanterm_backends.o -Ipack/lib/flanterm/src 

x86_64-orange-g++ -o "$1/usr/bin/init" pack/init.o pack/flanterm.o pack/flanterm_backends.o 
