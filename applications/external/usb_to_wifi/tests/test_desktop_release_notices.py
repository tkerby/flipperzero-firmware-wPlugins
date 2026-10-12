import unittest
from pathlib import Path
from scripts.desktop_release_notices import is_notice, source_page


class ReleaseNoticeTests(unittest.TestCase):
    def test_keeps_wheel_license_directories_and_non_license_named_texts(self):
        for value in (
            "pkg.dist-info/licenses/LGPL-3.0-only.txt",
            "LICENSES/MIT.txt",
            "COPYING",
            "NOTICE",
            "COPYRIGHT",
            "AUTHORS",
        ):
            self.assertTrue(is_notice(Path(value)), value)
        self.assertFalse(is_notice(Path("PySide6/QtWidgets.pyd")))

    def test_preserves_exact_ubuntu_source_version_in_download_directions(self):
        self.assertEqual(
            source_page("glib2.0", "2:2.80.0-6ubuntu3.4"),
            "https://launchpad.net/ubuntu/+source/glib2.0/2%3A2.80.0-6ubuntu3.4",
        )
