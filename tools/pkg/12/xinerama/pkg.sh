
# 

# https://archive.xfce.org/src/xfce/xfwm4/4.19/xfwm4-4.19.0.tar.bz2


. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack
cd pack

fast_install "$1" https://www.x.org/releases/individual/lib/libXinerama-1.1.5.tar.gz

cd ..