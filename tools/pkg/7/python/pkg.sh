
. ../../pkg-lib.sh

rm -rf pack

mkdir -p pack

cd pack

export ac_cv_file__dev_ptmx=yes 
export ac_cv_file__dev_ptc=yes 
export ac_cv_func_sched_setscheduler=no 
export ac_cv_buggy_getaddrinfo=no 

fast_install "$1" https://www.python.org/ftp/python/3.13.9/Python-3.13.9.tar.xz "--build=x86_64 --with-computed-gotos --disable-optimizations --disable-ipv6 --without-system-expat --enable-loadable-sqlite-extensions --without-ensurepip --with-tzpath=/usr/share/zoneinfo --with-build-python=python3 --without-static-libpython" "../../diff/python.diff"

ln -sfv python3 "$1/usr"/bin/python
ln -sfv python3-config "$1/usr"/bin/python-config
ln -sfv pydoc3 "$1/usr"/bin/pydoc
ln -sfv idle3 "$1/usr"/bin/idle

mv "$1/usr"/lib/python3.13 "$1/usr"/lib/python3

ln -sfv python3 "$1/usr"/lib/python3.13

# rm -r "$1"/usr/lib/python*/{test,ctypes/test,distutils/tests,idlelib/idle_test,lib2to3/tests,sqlite3/test,tkinter/test,unittest/test} # used from arch linux pkgbuild

cd ..