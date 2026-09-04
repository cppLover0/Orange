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

echo Launching dbus
(dbus-daemon --session --address=unix:path=/run/user/1000/bus) > /dev/null 2> /dev/null &
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus

PROFILE_ID=$(gsettings get org.gnome.Terminal.ProfilesList default | tr -d "'") 
gsettings set org.gnome.Terminal.Legacy.Profile:/org/gnome/terminal/legacy/profiles:/:$PROFILE_ID/ use-theme-colors false 
gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark' 

echo Launching mutter
xinit /bin/sh /etc/xinitrc > /dev/null 2> /dev/null