
stty -echo
echo Compiling glib schemas
glib-compile-schemas /usr/share/glib-2.0/schemas
echo Generating gdk loaders.cache
gdk-pixbuf-query-loaders > /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache
echo Launching MATE
chmod +x /etc/X11/materc
xinit /etc/X11/materc > /dev/null 2> /dev/null