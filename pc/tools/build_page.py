"""Bundle flipper_app/web/ into one index.html (build_page.sh gzips it).
Usage: python3 -I pc/tools/build_page.py [OUT_DIR]   (default: flipper_app/files)
The page order is listed in flipper_app/web/order.txt ("css <path>" / "js <path>").
"""
import os, sys

TOP = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
WEB = os.path.join(TOP, "flipper_app", "web")


def read(rel):
    with open(os.path.join(WEB, rel), encoding="utf-8", newline="") as f:
        return f.read()


def bundle():
    parts = {"css": [], "js": []}
    for line in read("order.txt").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        kind, path = line.split(None, 1)
        parts[kind].append(read(path))
    page = read("index.html")
    for key, kind in (("@@CSS@@", "css"), ("@@JS@@", "js")):
        text = "".join(parts[kind])
        if text.endswith("\n"):
            text = text[:-1]
        if page.count(key) != 1:
            sys.exit("build_page: placeholder %s must appear once in web/index.html" % key)
        page = page.replace(key, text)
    return page.encode("utf-8")


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(TOP, "flipper_app", "files")
    os.makedirs(out, exist_ok=True)
    data = bundle()
    with open(os.path.join(out, "index.html"), "wb") as f:
        f.write(data)

main()
