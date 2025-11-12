. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

git clone https://github.com/ozkl/doomgeneric.git --depth=1
cd doomgeneric

diff_patch ../../diff/doomgeneric.diff
cd doomgeneric
make -j$(nproc)

rm -rf "$1/usr/bin/doomgeneric"
cp -rf doomgeneric "$1/usr/bin/doomgeneric-x11"

cd ../