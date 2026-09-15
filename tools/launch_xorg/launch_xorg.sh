stty -echo

echo Generating dbus uuid
dbus-uuidgen --ensure=/var/lib/dbus/machine-id

echo Compiling glib schemas
glib-compile-schemas /usr/share/glib-2.0/schemas

export LC_ADDRESS=C.UTF-8
export LC_NAME=C.UTF-8
export LC_MONETARY=C.UTF-8
export LC_PAPER=C.UTF-8
export LC_IDENTIFICATION=C.UTF-8
export LC_TELEPHONE=C.UTF-8
export LC_MEASUREMENT=C.UTF-8
export LC_TIME=C.UTF-8
export LC_NUMERIC=C.UTF-8
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export DISPLAY=:0
export XDG_CURRENT_DESKTOP=GNOME 
export HOME=/root

echo Updating gdk-pixbuf

gdk-pixbuf-query-loaders --update-cache

echo Launching dbus
export GSK_RENDERER=cairo 

echo Launching dbus system

dbus-daemon --system --fork
export DBUS_SYSTEM_BUS_ADDRESS="unix:path=/run/dbus/system_bus_socket"

echo Launching dbus session

(dbus-daemon --session --address=unix:path=/run/user/1000/bus) > /dev/null 2> /dev/null &
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus

echo meow meow meow

PROFILE_ID=$(gsettings get org.gnome.Terminal.ProfilesList default | tr -d "'") 

gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark'
gsettings set org.gnome.desktop.interface icon-theme 'hicolor'

gsettings set org.gnome.desktop.background picture-options 'zoom'
gsettings set org.gnome.desktop.background picture-uri 'file:///etc/gnomebg.png'
gsettings set org.gnome.desktop.background picture-uri-dark 'file:///etc/gnomebg.png'

echo Launching gnome-shell
xinit /bin/sh /etc/xinitrc > /dev/null 2> /dev/null