


. ../pkg_lib.sh

rm -rf pack

mkdir -p pack

cd pack

echo $1

git clone https://github.com/ozkl/doomgeneric.git --depth=1
cd doomgeneric

diff_patch ../../diff/doomgeneric.diff
cd doomgeneric
make -j$(nproc)

cp -rf doomgeneric "$1/bin/doomgeneric"

old="$(pwd)"
cd "$1/share"
curl -O https://ia804501.us.archive.org/24/items/theultimatedoom_doom2_doom.wad/DOOM.WAD%20%28For%20GZDoom%29/DOOM.WAD
cd "$old"

cd ..

cd ../