
rm -rf pack

mkdir pack
cd pack

OLD="$(realpath .)"
NEW="$(realpath ../../../../)"

cd "$NEW"

rm -rf tools/orange-mlibc

cd tools
sh get-linux-headers.sh "$NEW"
git clone https://github.com/cpplover0/orange-mlibc --depth=1
cd orange-mlibc 
bash build_to_cross.sh "$NEW"

cd "$OLD"

