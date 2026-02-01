
export PATH="$HOME/opt/cross/orange/bin:$PATH"

#sh build-pkg.sh 0/glibc
cd tools
sh toolchain-build.sh "$(realpath ../)"
cd ../