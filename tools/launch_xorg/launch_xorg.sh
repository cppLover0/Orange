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

gsettings set org.gnome.Terminal.Legacy.Settings theme-variant 'dark'
gsettings set org.gnome.desktop.interface gtk-theme 'Orchis-Dark-Compact'

echo Launching dbus
(dbus-daemon --session --address=unix:path=/run/user/1000/bus) > /dev/null 2> /dev/null &
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus

echo Launching i3wm
xinit /bin/sh /etc/xinitrc > /dev/null 2> /dev/null