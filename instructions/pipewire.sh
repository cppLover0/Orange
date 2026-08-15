. "${pkg_lib}"

# keep in mind this is for stub
prepare() {
    autotools_recursive_regen
}

configure() {
    meson_configure -Dcpp_args="-D_GNU_SOURCE -D_USE_MATH_DEFINES" \
  -Ddocs=disabled \
  -Dtests=disabled \
  -Dexamples=disabled \
  -Dman=disabled \
  -Dalsa=disabled \
  -Dpipewire-alsa=disabled \
  -Dpipewire-jack=disabled \
  -Djack=disabled \
  -Davahi=disabled \
  -Dbluez5=disabled \
  -Dgstreamer=disabled \
  -Dlibsystemd=disabled \
  -Draop=disabled \
  -Dv4l2=disabled \
  -Dlibusb=disabled \
  -Dsdl2=disabled \
  -Dsndfile=disabled \
  -Dlibpulse=enabled
}

build() {
    meson compile -j$(nproc)
}

install() {
    DESTDIR="${dest_dir}" meson install --no-rebuild
}

pkg_work
exit