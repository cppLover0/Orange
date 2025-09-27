
rm -rf initrd

# if [ ! "$(which x86_64-orange-gcc)" ]; then
#     echo "It looks like you don't have the cross-compiler installed, or it isn't in your PATH."
#     echo "If you built your cross-compiler, add it to your PATH with:"
#     echo 'export PATH="$HOME/opt/cross/orange/bin:$PATH"'
#     echo 'Alternatively, you can build the cross-compiler with: sh build-cross.sh'
#     echo 'Also you should have host gcc with version < 14 (i am using 13.3.0)'
#     exit 1
# fi

chmod +x jinx

mkdir -p initrd/lib initrd/bin initrd/usr/bin initrd/usr/lib

script_dir="$(dirname "$0")"
source_dir="$(cd "${script_dir}" && pwd -P)"

chmod +x "${source_dir}/jinx"

cd "${source_dir}/build-x86_64"
"${source_dir}"/jinx update base 
"${source_dir}"/jinx install "sysroot" base 

# cd initrd/lib

# echo Creating symlinks ./*.so
# for file in ../usr/lib/*.so; do
# 	echo $file "$(basename "$file")"
#     ln -s "$file" "$(basename "$file")"
# done

# for file in ../usr/local/lib/*.so; do
# 	echo $file "$(basename "$file")"
#     ln -s "$file" "$(basename "$file")"
# done

# cd ../bin

# echo Creating symlinks ./*
# for file in ../usr/bin/*; do
# 	echo $file "$(basename "$file")"
#     ln -sf "$file" "$(basename "$file")"
# done

# for file in ../usr/local/bin/*; do
# 	echo $file "$(basename "$file")"
#     ln -sf "$file" "$(basename "$file")"
# done

# cd ../../

# cp -rf tools/initbase/* initrd

# mkdir -p tools/base/boot
# tar -cf tools/base/boot/initrd.tar -C initrd .
