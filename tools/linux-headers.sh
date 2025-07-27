if [ ! -d "./linux-headers" ]; then
    mkdir -p temp
    mkdir -p linux-headers
    git clone https://github.com/torvalds/linux.git --depth=1
    cd linux
    make headers_install ARCH=x86_64 INSTALL_HDR_PATH="$(realpath ../temp)"
    cd ..
    cp -rf temp/include/* linux-headers
    rm -rf temp
    rm -rf linux
fi

echo Copying linux-headers...
mkdir -p "$1/initrd/usr/include"
cp -rf linux-headers/* "$1/initrd/usr/include"