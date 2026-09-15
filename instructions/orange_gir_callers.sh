. "${pkg_lib}"

prepare() {
    true
}

configure() {
    true
}

build() {
    true
}

install() {
    cp -rf "${source_dir}"/o-orange-gir-scanner "${source_dir}"/o-orange-gir-compiler "${source_dir}"/o-orange-gir-generate "${host_dest_dir}"/bin
    chmod +x "${host_dest_dir}"/bin/o-orange-gir-scanner 
    chmod +x "${host_dest_dir}"/bin/o-orange-gir-compiler 
    chmod +x "${host_dest_dir}"/bin/o-orange-gir-generate 
}

pkg_work
exit