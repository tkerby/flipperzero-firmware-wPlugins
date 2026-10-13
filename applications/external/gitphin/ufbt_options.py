# uFBT options for this app (read by ufbt automatically).
#
# Gitphin adds a second USB serial port while it runs, so ufbt's auto-detect
# reports "More than one Flipper is attached". Pin the CLI port (if00) so
# `ufbt launch` always targets the right port, whether or not the app is running.
#
# Set GITPHIN_FLIP_PORT to choose the port yourself (several Flippers attached);
# otherwise the single attached Flipper's CLI port is used. With no Flipper
# attached (CI, catalog builds) nothing is set.
import glob as _glob
import os as _os

_ports = sorted(_glob.glob("/dev/serial/by-id/usb-Flipper_Devices_Inc.*-if00"))
if _os.environ.get("GITPHIN_FLIP_PORT"):
    FLIP_PORT = _os.environ["GITPHIN_FLIP_PORT"]
elif len(_ports) == 1:
    FLIP_PORT = _ports[0]
