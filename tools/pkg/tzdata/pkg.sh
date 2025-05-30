
. ../pkg_lib.sh

rm -rf pack
mkdir -p pack

cd pack

wget https://data.iana.org/time-zones/releases/tzdb-2025b.tar.lz
tar -xvf tzdb-2025b.tar.lz

cd tzdb-2025b

make install TOPDIR=$(realpath "$1/..") СС=x86_64-orange-gcc LD=x86_64-orange-ld

rm -rf "$1/share/zoneinfo-posix" "$1/share/zoneinfo-leaps"  "$1/share/zoneinfo"

cd ..