"""Attach actual binary notices and corresponding-source directions to releases.

Called only while packaging on a target OS. Failure stops public package creation;
it never changes runtime permissions, app code or the project's MIT license.
"""
from __future__ import annotations

import ctypes
import hashlib
import importlib.metadata
import json
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.parse
import urllib.request
from pathlib import Path


def is_notice(path: Path) -> bool:
    """Wheel LICENSES directories include LGPL/GPL files without 'license'."""
    words = ("license", "copying", "notice", "copyright", "authors", "lgpl", "gpl")
    return any(word in str(path).lower() for word in words)


def source_page(package: str, version: str) -> str:
    return ("https://launchpad.net/ubuntu/+source/"
            + urllib.parse.quote(package, safe="") + "/"
            + urllib.parse.quote(version, safe=""))


def _download(url: str, destination: Path) -> None:
    # No certificate bypass, silent fallback or made-up source version.
    with urllib.request.urlopen(url, timeout=60) as response, destination.open("wb") as output:
        shutil.copyfileobj(response, output)


def _qt_sources(target: Path) -> list[dict]:
    from PySide6.QtCore import qVersion
    qt_version = qVersion()
    pyside_version = importlib.metadata.version("PySide6-Essentials")
    if not re.fullmatch(r"\d+\.\d+\.\d+", qt_version):
        raise ValueError("Release packaging requires a stable, exact Qt version")
    root = Path(__file__).resolve().parents[1]
    downloads = root / ".build-tests/third-party-sources"
    downloads.mkdir(parents=True, exist_ok=True)
    major_minor = ".".join(qt_version.split(".")[:2])
    result = []
    modules = ["qtbase", "qtsvg", "qtimageformats"]
    if platform.system() == "Linux":
        modules.append("qtwayland")
    entries = [(module, qt_version,
        f"https://download.qt.io/official_releases/qt/{major_minor}/{qt_version}/submodules/{module}-everywhere-src-{qt_version}.tar.xz")
        for module in modules]
    entries.append(("pyside-setup", pyside_version,
        f"https://download.qt.io/official_releases/QtForPython/pyside6/PySide6-{pyside_version}-src/pyside-setup-everywhere-src-{pyside_version}.tar.xz"))
    for module, version, url in entries:
        archive = downloads / url.rsplit("/", 1)[1]
        if not archive.exists():
            _download(url, archive)
        notices = target / "licenses/upstream-sources" / module
        # Read only regular notice/attribution files; never extract arbitrary paths.
        with tarfile.open(archive, "r:xz") as source:
            members = source.getmembers()
            for item in members:
                path = Path(item.name)
                if (not item.isfile() or item.size > 2 * 1024 * 1024
                        or path.is_absolute() or ".." in path.parts):
                    continue
                if not is_notice(path) and path.name != "qt_attribution.json":
                    continue
                destination = notices / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                stream = source.extractfile(item)
                if stream is not None:
                    with stream, destination.open("wb") as output:
                        shutil.copyfileobj(stream, output)
        result.append({"component": module, "version": version,
            "source": url, "release_source_asset": archive.name,
            "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()})
    return result


def _linux_notices(target: Path) -> list[dict]:
    result = []
    packages = {}
    internal = target / "_internal"
    common = Path("/usr/share/common-licenses")
    if common.is_dir():
        shutil.copytree(common, target / "licenses/system/common-licenses", dirs_exist_ok=True)
    for library in sorted(internal.glob("*.so*")):
        name = library.name
        if not name.startswith("lib"):
            continue  # Python extension wheel notices were copied by copy_notices.
        if name.startswith(("libQt6", "libpyside", "libshiboken", "libpython")):
            continue  # Exact Qt/PySide and CPython sources are recorded separately.
        if name.startswith("libreadline"):
            raise ValueError("Unused GPL readline must not be bundled")
        if name.startswith(("libssl", "libcrypto")):
            # setup-python may ship a different OpenSSL build than Ubuntu's dpkg.
            strings = subprocess.check_output(["strings", str(library)], text=True)
            match = re.search(r"OpenSSL (\d+\.\d+\.\d+[a-z]?)", strings)
            if match is None:
                raise ValueError("Cannot identify bundled OpenSSL version: " + name)
            version = match.group(1)
            result.append({"component": "OpenSSL", "version": version,
                "libraries": [name],
                "source": f"https://github.com/openssl/openssl/releases/tag/openssl-{version}"})
            continue
        if name.startswith("libicu"):
            major = name.rsplit(".", 1)[-1]
            if name.startswith("libicuuc"):
                lib = ctypes.CDLL(str(library.resolve()))
                version = (ctypes.c_uint8 * 4)()
                function = getattr(lib, "u_getVersion_" + major)
                function.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
                function.restype = None
                function(version)
                tag = "-".join(str(part) for part in list(version)[:2])
                result.append({"component": "ICU", "version": tag.replace("-", "."),
                    "source": f"https://github.com/unicode-org/icu/releases/tag/release-{tag}"})
                license_path = target / "licenses/system/ICU-LICENSE"
                _download(f"https://raw.githubusercontent.com/unicode-org/icu/release-{tag}/icu4c/LICENSE", license_path)
            continue
        # Query the native runner, not a guessed distribution/package version.
        query = subprocess.run(["dpkg-query", "-S", "*/" + name],
                               capture_output=True, text=True)
        candidates = [line.rsplit(": ", 1)[0] for line in query.stdout.splitlines()
                      if ": " in line]
        if not candidates:
            raise ValueError("Cannot identify bundled system library: " + name)
        package = candidates[0]
        if package not in packages:
            info = subprocess.check_output(["dpkg-query", "-W", "-f",
                "${binary:Package}\t${Version}\t${source:Package}\t${source:Version}", package], text=True)
            binary, binary_version, source, source_version = info.split("\t")
            copyright_path = Path("/usr/share/doc") / binary.split(":", 1)[0] / "copyright"
            if not copyright_path.is_file():
                raise ValueError("Missing native copyright notice: " + binary)
            destination = target / "licenses/system" / binary / "copyright"
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(copyright_path, destination)
            packages[package] = {"component": binary, "version": binary_version,
                "source_package": source, "source_version": source_version,
                "source": source_page(source, source_version), "libraries": []}
        packages[package]["libraries"].append(name)
    return result + list(packages.values())


def collect_release_notices(target: Path) -> None:
    records = _qt_sources(target)
    python_version = platform.python_version()
    python_license = target / "licenses/CPython-LICENSE"
    python_license.parent.mkdir(parents=True, exist_ok=True)
    installed_license = Path(sys.base_prefix) / "LICENSE.txt"
    if installed_license.is_file():
        shutil.copy2(installed_license, python_license)
    else:
        _download(f"https://raw.githubusercontent.com/python/cpython/v{python_version}/LICENSE", python_license)
    records.append({"component": "CPython", "version": python_version,
        "source": f"https://www.python.org/ftp/python/{python_version}/Python-{python_version}.tar.xz"})
    # OpenSSL 3 license applies to the dynamically shipped OpenSSL DLLs as well.
    _download("https://raw.githubusercontent.com/openssl/openssl/openssl-3.0.0/LICENSE.txt",
              target / "licenses/OpenSSL-3-LICENSE.txt")
    if platform.system() == "Linux":
        records.extend(_linux_notices(target))
    (target / "native-sources.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    (target / "SOURCES.md").write_text(
        "# Third-party notices and corresponding sources\n\n"
        "This application uses dynamically linked Qt, PySide6 and Shiboken6 under LGPLv3. "
        "Their full license texts and third-party notices are in `licenses/`. "
        "The application's own code remains MIT licensed. OpenSSL is by the OpenSSL Project.\n\n"
        "Keep the entire directory. You may replace/rebuild LGPL libraries and debug or "
        "reverse-engineer the application for those modifications; no restriction or "
        "signature lock prevents this. Keep replacement ABI and architecture compatible "
        "and replace shared libraries in `_internal` (including linked copies).\n\n"
        "The exact Qt/PySide source archives listed in `native-sources.json` are also "
        "attached to this GitHub release, with SHA-256 hashes. They include upstream "
        "build scripts, third-party sources and notices; these libraries were not patched "
        "by this project. Upstream source URLs remain available as an alternative.\n\n"
        "For each Ubuntu system dependency, open its exact-version Launchpad source "
        "page recorded in `native-sources.json`, then download all source-package files "
        "(.dsc, original source tarballs and Debian/Ubuntu patches). Package copyright "
        "files and common licenses are under `licenses/system/`. ICU/CPython source "
        "versions are listed separately. Downloading sources is free and does not "
        "require this application or a GitHub account.\n\n"
        "Build Qt: https://doc.qt.io/qt-6/build-sources.html\n\n"
        "Build PySide/Shiboken: https://doc.qt.io/qtforpython-6/building_from_source/index.html\n\n"
        "Build this MIT application: https://github.com/mete888/flipper_internet_bridge/blob/main/host/README.md\n",
        encoding="utf-8")
