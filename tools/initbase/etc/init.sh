export TERM=linux
export XDG_RUNTIME_DIR=/run/user/0
export XDG_CONFIG_DIRS=/etc/xdg
dbus-uuidgen --ensure
dbus-daemon --session --address=unix:path=/run/user/1000/bus &
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
orangeX