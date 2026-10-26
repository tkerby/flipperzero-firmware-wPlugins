# Catalog validation — 2026-10-10

Source commit: c99ef5a065ca9d0209cde4f96300906b8537e4da
App: BusterMan Decoder 1.0
Catalog path: applications/Tools/megaman/manifest.yml
SDK: official release 1.4.3, f7, API 87.1

Passed: python3 tools/bundle.py --nolint applications/Tools/megaman/manifest.yml bundle.zip

The catalog tool cloned the public source, checked out the pinned commit, built it, read FAM metadata, validated descriptions and assets, and created the bundle. All six original qFlipper captures are unchanged 512x256 RGB images with exactly black and orange (254,138,44). icon.png is 10x10, PNG mode 1, without transparency; adapted from the embedded splash artwork. Existing appid is unique in the catalog checkout. C/header logic is unchanged from the previously checked source.

Physical device acceptance of the prior renamed build was reported by the owner. The final metadata/icon rebuild has not separately been run on hardware here. The catalog submission is now open as [PR #1300](https://github.com/flipperdevices/flipper-application-catalog/pull/1300); this validation record applies to the source commit listed above.
