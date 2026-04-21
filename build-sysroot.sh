source_dir="$1"
chmod +x "${source_dir}"/jinx

echo building arch ${ARCH}
if ! [ -f .jinx-parameters ]; then
    echo init jinx
    "${source_dir}"/jinx init "${source_dir}" ARCH="${ARCH}"
fi

echo update jinx
"${source_dir}"/jinx update "*"
echo install jinx

JINX_ARCH=$ARCH "${source_dir}"/jinx install "sysroot" "*"
