
GNU_MIRROR=https://mirror.dogado.de/

cachedownload() {
    if [ ! -d "../cached/$1" ]; then
        curl -o ../cached/$1 $2
    fi
}

installgnu() {
    cachedownload ../cached/$2-$3.tar.gz "$GNU_MIRROR/gnu/$1/$2-$3.tar.gz"
    rm -rf ./*
    cp -rf ../cached/$2-$3.tar.gz .
    tar -xvf $2-$3.tar.gz
}

diff_patch() {
    patch -p1 < "$1"
}