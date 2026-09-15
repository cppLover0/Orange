. "${pkg_lib}"

prepare() {
    autotools_recursive_regen
}

configure() {
    "${source_dir}"/configure --host=x86_64-orange-mlibc --prefix=/usr 
}

build() {
    cat > src/lock-obj-pub.native.h <<'EOF'
## lock-obj-pub.x86_64-pc-orange-mlibc.h
## File created for Orange/mlibc - DO NOT EDIT
## mlibc pthread_mutex_t is 64 bytes, so the public lock object
## must be at least as large as the internal _gpgrt_lock_t.

typedef struct
{
  long _vers;
  union {
    volatile char _priv[64];
    long _x_align;
    long *_xp_align;
  } u;
} gpgrt_lock_t;

#define GPGRT_LOCK_INITIALIZER {1,{{0}}}
##
## Local Variables:
## mode: c
## buffer-read-only: t
## End:
##
EOF
    make -j$(nproc)
}

install() {
    make install DESTDIR="${dest_dir}"
}

pkg_work
exit