
sh build-pkg.sh 0/mlibc
cd tools
sh toolchain-build.sh "$(realpath ../)"
cd ../