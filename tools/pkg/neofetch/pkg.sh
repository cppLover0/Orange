
. ../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack
mkdir neofetch
cd neofetch
wget https://raw.githubusercontent.com/dylanaraps/neofetch/refs/heads/master/neofetch
cd ..

cp -rf neofetch/neofetch "$1/bin"

cd ..