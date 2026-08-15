# runs converted orange to linux binary

_path="$(realpath $1)"
shift 1

dest_dir="$(realpath orange_sysroot/usr/lib)"
host_dest_dir="$(realpath .orange-build/prefix/mlibc-host/usr/lib)"

export LD_LIBRARY_PATH="${host_dest_dir}:${dest_dir}:${dest_dir}/mutter-8"
echo "${LD_LIBRARY_PATH}"
"${_path}" "$@"