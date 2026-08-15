# converts orange binaries to linux 
# first arg is input, second is output

_path="$1"

_temp="$2"
cp -rf "${_path}" "${_temp}"
execstack -c "${_temp}"
patchelf --set-interpreter "$(realpath .orange-build/prefix/mlibc-host/usr/lib/ld.so)" "${_temp}"
