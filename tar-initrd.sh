
rm -rf initrd

export PATH="$HOME/opt/cross/orange/bin:$PATH"

if [ ! "$(which x86_64-orange-gcc)" ]; then
    echo "It looks like you don't have the cross-compiler installed, or it isn't in your PATH."
    echo "If you built your cross-compiler, add it to your PATH with:"
    echo 'export PATH="$HOME/opt/cross/orange/bin:$PATH"'
    echo 'Alternatively, you can build the cross-compiler with: sh build-cross.sh'
    echo 'Also you should have host gcc with version < 14 (i am using 13.3.0)'
    exit 1
fi


bash tools/pkg/build-pkg.sh "$(realpath initrd)"

cd initrd

rm -rf lib lib64 bin

ln -sf usr/lib lib
ln -sf usr/lib lib64
ln -sf usr/bin bin

cd ../

cp -rf tools/initbase/* initrd

mkdir -p tools/base/boot
tar -cf tools/base/boot/initrd.tar -C initrd .
