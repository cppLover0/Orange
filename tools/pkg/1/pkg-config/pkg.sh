# https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz


. ../../pkg-lib.sh

mkdir -p cached

rm -rf pack

mkdir -p pack

cd pack

wget https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz
tar -xvf pkg-config-0.29.2.tar.gz
cd pkg-config-0.29.2

diff_patch ../../diff/pkg-config.diff 
./configure --prefix="$HOME/opt/cross/orange" --with-internal-glib --target=x86_64-orange
make -j$(nproc)
make install

mkdir -p "$HOME/opt/cross/orange"/share/pkgconfig/personality.d
	
cat > "$HOME/opt/cross/orange"/share/pkgconfig/personality.d/x86_64-orange.personality << EOF
Triplet: x86_64-orange
SysrootDir: $1
DefaultSearchPaths: $1/usr/lib/pkgconfig:$1/usr/share/pkgconfig
SystemIncludePaths: $1/usr/include
SystemLibraryPaths: $1/usr/lib
EOF

c="$(pwd)"

cd "$HOME/opt/cross/orange/bin"

ln -sf pkg-config x86_64-orange-mlibc-pkg-config

cd "$c"

# wget https://ftpmirror.gnu.org/gnu/libtool/libtool-2.5.4.tar.gz
# tar -xvf libtool-2.5.4.tar.gz
# cd libtool-2.5.4

# diff_patch ../../diff/libtool.diff
# ./configure --prefix="$HOME/opt/cross/orange" --with-gnu-ld --enable-shared --disable-static
# make -j$(nproc)
# make install

# cd ..

cd ..
