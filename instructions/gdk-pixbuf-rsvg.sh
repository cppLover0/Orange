. "${pkg_lib}"

exit 0

#ill leave this recipe for future maybe someday ill need it

gir_prepare

prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dgio_sniffing=true -Dman=false -Dintrospection=enabled -Dbuiltin_loaders=all 
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit