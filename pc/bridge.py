#!/usr/bin/env python3
"""Gitphin bridge: http://localhost:8080 <-> HTTP server on the Flipper.

Usage: .venv/bin/python -I pc/bridge.py [--port 8080] [--open-browser]

The HTTP server runs on the Flipper (Apps > Tools > Gitphin). This bridge:
  1. starts the Flipper app through the CLI port when the Flipper is plugged in,
  2. carries each TCP connection as frames over the app's USB serial channel,
  3. answers the PC-side /pc/* routes itself (Collector, ZIP download, webhooks, backup),
  4. sends webhooks when refs change and mirrors the repos to the PC on plug-in.
It exits when the Flipper is unplugged; systemd starts it again on the next plug.

The code is in pc/gitphin_bridge/ (one module per job). This file only starts it.
"""

import os
import sys

# The service runs this with `python -I`, which drops the script's own directory
# from sys.path, so add it back before importing the package.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gitphin_bridge.main import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
