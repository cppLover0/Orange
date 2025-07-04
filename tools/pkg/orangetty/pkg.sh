
rm -rf pack/*
mkdir -p pack/lib/

cd pack/lib/
git clone https://codeberg.org/mintsuki/flanterm.git
cd ../../

x86_64-orange-g++ -c src/main.cpp -o "pack/orangetty.o" -Ipack/lib/flanterm/src -Isrc/include -O2
x86_64-orange-gcc -c pack/lib/flanterm/src/flanterm.c -o pack/flanterm.o -Ipack/lib/flanterm/src -O2
x86_64-orange-gcc -c pack/lib/flanterm/src/flanterm_backends/fb.c -o pack/flanterm_backends.o -Ipack/lib/flanterm/src -O2

x86_64-orange-g++ -o "$1/bin/orangetty" pack/orangetty.o pack/flanterm.o pack/flanterm_backends.o -O2