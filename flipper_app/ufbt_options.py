# uFBT options for this app (read by ufbt automatically).
#
# Flipper Hub adds a second USB serial port while it runs, so ufbt's auto-detect
# reports "More than one Flipper is attached". Pin the CLI port (if00) so
# `ufbt launch` always targets the right port, whether or not the app is running.
#
# This path is specific to this Flipper (name "Rihayo"). If you use another
# Flipper, change it, or remove this file and close the app before `ufbt launch`.
FLIP_PORT = "/dev/serial/by-id/usb-Flipper_Devices_Inc._Flipper_Rihayo_flip_Rihayo-if00"
