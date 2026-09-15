. "${pkg_lib}"

# tzdata from the eggert/tz mirror (same data as the IANA release, but shipped
# as a single top-level directory so orangestrap's extractor can handle it).
# The zoneinfo files are arch-independent TZif data, compiled by the host zic.

prepare() {
    true
}

configure() {
    true
}

build() {
    mkdir -p "${build_dir}/zoneinfo" \
             "${build_dir}/zoneinfo/posix" \
             "${build_dir}/zoneinfo/right"

    command -v zic >/dev/null 2>&1 || export PATH="/usr/sbin:${PATH}"

    echo building tzdata

    cd "${source_dir}"

    awk -v EXPIRES_LINE= \
        -f leapseconds.awk leap-seconds.list > leapseconds

    zic -d "${build_dir}/zoneinfo" \
        africa antarctica asia australasia europe northamerica \
        southamerica etcetera factory backward

    zic -d "${build_dir}/zoneinfo/posix" \
        africa antarctica asia australasia europe northamerica \
        southamerica etcetera factory backward

    zic -d "${build_dir}/zoneinfo/right" -L leapseconds \
        africa antarctica asia australasia europe northamerica \
        southamerica etcetera factory backward

    cp -f iso3166.tab zone.tab zone1970.tab zonenow.tab \
          "${build_dir}/zoneinfo/"
}

install() {

    echo installing it

    rm -rf "${dest_dir}/usr/share/zoneinfo"
    mkdir -p "${dest_dir}/usr/share/zoneinfo"
    cp -rf "${build_dir}"/zoneinfo/* "${dest_dir}/usr/share/zoneinfo/"
    cp -f "${source_dir}"/zone.tab "${dest_dir}/usr/share/zoneinfo/"

    rm -f "${dest_dir}/etc/localtime"
    ln -s /usr/share/zoneinfo/UTC "${dest_dir}/etc/localtime"
}

pkg_work
exit
