. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
   LDFLAGS="-lintl" meson_configure -Ddaemon=false \
  -Dclient=true \
  -Dalsa=disabled \
  -Dglib=enabled \
  -Dudev=disabled \
  -Davahi=disabled \
  -Dbluez5=disabled \
  -Dgtk=disabled \
  -Dopenssl=disabled \
  -Dorc=disabled \
  -Dsamplerate=disabled \
  -Ddbus=enabled \
  -Dx11=disabled \
  -Dtests=false \
  -Dman=false
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit