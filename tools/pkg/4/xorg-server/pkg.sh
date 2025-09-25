
# https://www.x.org/releases/X11R7.7/src/xserver/xorg-server-1.12.2.tar.gz

. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

wget https://www.x.org/releases/X11R7.7/src/xserver/xorg-server-1.12.2.tar.gz

tar -xvf xorg-server-1.12.2.tar.gz

cd xorg-server-1.12.2
diff_patch ../../diff/xorg-server.diff
./configure CFLAGS="$TARGET_CFLAGS -Wno-error=array-bounds" \
        --with-xkb-bin-directory=/usr/bin \
        --with-xkb-path=/usr/share/X11/xkb \
        --with-xkb-output=/var/lib/xkb \
        --prefix=/usr \
        --with-fontrootdir=/usr/share/fonts/X11 \
        --enable-xorg \
        --enable-xv \
        --enable-xvfb \
        --disable-xephyr \
        --disable-xnest \
        --disable-suid-wrapper \
        --disable-pciaccess \
        --disable-dpms \
        --enable-screensaver \
        --disable-xres \
        --disable-xvmc \
        --disable-systemd-logind \
        --disable-secure-rpc \
        --disable-config-udev \
        --disable-dri \
        --disable-dri2 \
        --disable-dri3 \
        --disable-int10-module \
        --disable-vgahw \
        --disable-libdrm \
        --disable-glamor \
        --disable-glx

make -j$(nproc)
make install -j$(nproc) DESTDIR="$1"

cd ..