# Gitphin

**Git hosting in your pocket.** Gitphin turns your Flipper Zero into a Git server with a web UI. Your repositories live on the SD card, and you use them from your PC over the USB cable with plain git: clone, pull and push.

## What you get

- A real Git remote: **git clone**, **git pull** and **git push** work with the git you already have
- A web UI in your browser at http://localhost:8080: code, commits, diffs, branches, tags and search
- **Issues, pull requests, releases and a wiki** for every repository
- Normal bare repositories in /ext/git on the SD card: copy them to any PC
- Offline and private: no server, no cloud, no account

## What you need

- The **Gitphin bridge** on a **Linux PC**: a small Python program that connects the browser and git to the Flipper over USB. Download it from the project page: https://github.com/Adhamura/Gitphin
- git and a web browser on the PC

## How to use

1. Install the bridge on the PC (one command, see the project page) and plug in the Flipper
2. The bridge starts Gitphin on the Flipper by itself
3. Push a repo: git remote add flipper http://127.0.0.1:8080/git/myproject.git, then git push flipper main
4. Open http://localhost:8080 in your browser

## Tips

- Close qFlipper and Flipper Lab while you use Gitphin: they hold the USB port
- Turn the SD desktop animations off for more free memory
- Do not exit Gitphin while it is busy: reboot the Flipper instead (Left + Back), your repos stay safe

Not affiliated with Flipper Devices. Open source, MIT license: https://github.com/Adhamura/Gitphin
