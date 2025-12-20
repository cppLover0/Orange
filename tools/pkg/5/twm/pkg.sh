
. ../../pkg-lib.sh

rm -rf pack
mkdir -p pack

cd pack

fast_install "$1" https://www.x.org/releases/individual/lib/libXext-1.3.4.tar.gz
fast_install "$1" https://www.x.org/releases/individual/lib/libICE-1.1.0.tar.gz
fast_install "$1" https://www.x.org/releases/individual/lib/libSM-1.2.4.tar.gz
fast_install "$1" https://www.x.org/releases/individual/lib/libXt-1.2.1.tar.gz "--enable-shared" "../../diff/libxt.diff"
fast_install "$1" https://www.x.org/releases/individual/lib/libXmu-1.1.3.tar.gz

fast_install "$1" https://www.x.org/archive//individual/app/twm-1.0.11.tar.gz

fast_install "$1" https://www.x.org/releases/individual/lib/libXpm-3.5.14.tar.gz
fast_install "$1" https://www.x.org/releases/individual/lib/libXaw-1.0.14.tar.gz

fast_install "$1" https://github.com/libexpat/libexpat/releases/download/R_2_4_9/expat-2.4.9.tar.xz
fast_install "$1" https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.13.94.tar.gz  "--enable-shared" "../../diff/fontconfig.diff"
fast_install "$1" https://www.x.org/releases/individual/lib/libXrender-0.9.11.tar.gz
fast_install "$1" https://www.x.org/releases/individual/lib/libXft-2.3.4.tar.gz

fast_install "$1" https://www.x.org/releases/individual/lib/libXi-1.8.1.tar.gz

fast_install "$1" https://www.x.org/pub/individual/app/xclock-1.1.1.tar.xz
fast_install "$1" https://www.x.org/releases/individual/app/xeyes-1.2.0.tar.gz

fast_install "$1" https://www.x.org/releases/individual/app/xinit-1.4.1.tar.gz
