
GNU_MIRROR=https://mirror.dogado.de/

clear_share() {
    echo no
}

install_gnu() {

    pkg_name=$1
    name=$2
    version=$3

    wget -nc "$GNU_MIRROR/gnu/$pkg_name/$name-$version.tar.gz"
	tar -xvf "$name-$version.tar.gz"

    rm -rf "$name-$version.tar.gz"

}

diff_patch() {
    patch -p1 < "$1"
}