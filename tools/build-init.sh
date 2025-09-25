
ARCH=x86_64

script_dir="$(dirname "$0")"
source_dir="$(cd "${script_dir}"/.. && pwd -P)"

chmod +x "${source_dir}/jinx"

mkdir -p "${source_dir}/build-$ARCH" && cd "${source_dir}/build-$ARCH"
"${source_dir}"/jinx init "${source_dir}" ARCH=$ARCH