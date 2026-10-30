# Changelog

## [0.1.31](https://github.com/Endika/flipper-wifi-census/compare/v0.1.30...v0.1.31) (2026-09-20)


### Documentation

* one short description, the one the catalog listing shows ([4ac40cb](https://github.com/Endika/flipper-wifi-census/commit/4ac40cbd8e2f0e5e9a5c0e6a0e1a2b3c4d5e6f70))

## [0.1.30](https://github.com/Endika/flipper-wifi-census/compare/v0.1.29...v0.1.30) (2026-09-20)


### Features

* compare the networks two captures seek, not just their devices ([35bf469](https://github.com/Endika/flipper-wifi-census/commit/35bf4692dceca575108986b34f7eac9795458475))

## [0.1.29](https://github.com/Endika/flipper-wifi-census/compare/v0.1.28...v0.1.29) (2026-09-20)


### Performance Improvements

* cheaper live stats, block reads off the SD, and the whole MAC on screen ([39abcf8](https://github.com/Endika/flipper-wifi-census/commit/39abcf85f28cd0c5d133f4ca4abaa327f70edbaa))

## [0.1.28](https://github.com/Endika/flipper-wifi-census/compare/v0.1.27...v0.1.28) (2026-09-20)


### Bug Fixes

* close the rest of the whole-tree review, from the census race to the vendor table ([53e4c75](https://github.com/Endika/flipper-wifi-census/commit/53e4c754efc6247e27e0647b4e70ba3ac337426d))

## [0.1.27](https://github.com/Endika/flipper-wifi-census/compare/v0.1.26...v0.1.27) (2026-09-20)


### Bug Fixes

* stop losing data on a refused write, a failed read or a mistaken label ([a337a6a](https://github.com/Endika/flipper-wifi-census/commit/a337a6a53d30823cd75f1dbde32e4ee8d7210821))

## [0.1.26](https://github.com/Endika/flipper-wifi-census/compare/v0.1.25...v0.1.26) (2026-09-20)


### Bug Fixes

* an overflowing compare list, a wrong late link, and v1 captures merged at the wrong stride ([67d3eea](https://github.com/Endika/flipper-wifi-census/commit/67d3eeaaf1077febd399fd428ed33274b4ccd7e5))
* stream a stored capture instead of holding it whole beside its census ([04ebd1f](https://github.com/Endika/flipper-wifi-census/commit/04ebd1fc2cacf1a837c53c2e9b546872fca999e2))

## [0.1.25](https://github.com/Endika/flipper-wifi-census/compare/v0.1.24...v0.1.25) (2026-09-20)


### Bug Fixes

* no silent ceilings in the host tools, and no fixed one at all in wc_clean ([7c6aa4d](https://github.com/Endika/flipper-wifi-census/commit/7c6aa4d097ed4e2ef4effcf38959dca962e5eeb1))

## [0.1.24](https://github.com/Endika/flipper-wifi-census/compare/v0.1.23...v0.1.24) (2026-09-20)


### Features

* compare more than two captures and say who keeps coming back ([6cc8156](https://github.com/Endika/flipper-wifi-census/commit/6cc8156e82278238dbec0db4462290402435c15d))
* undo MAC rotation in a long capture, and measure its own error rate ([6df0b07](https://github.com/Endika/flipper-wifi-census/commit/6df0b074ffa03982f72acb35e1b76b16a5982462))

## [0.1.23](https://github.com/Endika/flipper-wifi-census/compare/v0.1.22...v0.1.23) (2026-09-20)


### Features

* compare two captures on a PC, past the Flipper's 100-device limit ([8c3cfa7](https://github.com/Endika/flipper-wifi-census/commit/8c3cfa79efc3fd8ffe1105ae8a00ebd28c341a83))
* decide auto-save from the menu and scroll long list entries ([656156e](https://github.com/Endika/flipper-wifi-census/commit/656156e411f48ff52bfe9df4e68c279c90629fb8))
* flip auto-save with left and right on the Scan row itself ([07d1873](https://github.com/Endika/flipper-wifi-census/commit/07d1873dffdcb04fa23bd17e0d6b870083e4024a))
* let the host tools write captures, not just CSV ([211ca41](https://github.com/Endika/flipper-wifi-census/commit/211ca412e2b3ff91f4ff0b37107dfc8573bd38bd))
* say how many devices a merge dropped at the ceiling ([950af4b](https://github.com/Endika/flipper-wifi-census/commit/950af4b1ff8a612be81b05d5a894e7b6ac0c1f10))
* summarize a stored capture and bound the phones behind its random MACs ([798f201](https://github.com/Endika/flipper-wifi-census/commit/798f2016b63bed7893ad56c265815e8f14799428))


### Bug Fixes

* a wifi name shared by two devices is a place, not an identity ([eeeb4fe](https://github.com/Endika/flipper-wifi-census/commit/eeeb4fe3818f9aaddb99aff8769cc12401e4d875))
* lint the list view for real, and reject an oversized settings file ([e169cd5](https://github.com/Endika/flipper-wifi-census/commit/e169cd5d3f8857abc8f922bc9bb2b45005b13312))
* never hide what was cut, from the pcap tools to every list on screen ([3c45e09](https://github.com/Endika/flipper-wifi-census/commit/3c45e09e481deb93ce098a7be98ee183a791a9b6))
* say what a capture, a merge or a comparison could not show ([ca4713e](https://github.com/Endika/flipper-wifi-census/commit/ca4713e03287b8994075f23f9ee95c91dc593c52))


### Performance Improvements

* render list rows from the census instead of storing a copy of every label ([e47305b](https://github.com/Endika/flipper-wifi-census/commit/e47305bb7af73e39a2ddc957762629fc41294455))
* stream captures off the SD and raise the device ceiling to 320 ([4745979](https://github.com/Endika/flipper-wifi-census/commit/47459795a37bbd19768fa2ee2363df62df5f13a2))

## [0.1.22](https://github.com/Endika/flipper-wifi-census/compare/v0.1.21...v0.1.22) (2026-09-20)


### Bug Fixes

* report a busy UART instead of crashing, and list every device in a capture ([940cdbf](https://github.com/Endika/flipper-wifi-census/commit/940cdbff8d4cf26b8fd9c30c86c1ae285e232686))

## [0.1.21](https://github.com/Endika/flipper-wifi-census/compare/v0.1.20...v0.1.21) (2026-09-20)


### Features

* pick the import pcap from anywhere on the SD via the file browser ([6121004](https://github.com/Endika/flipper-wifi-census/commit/61210045ceff405a965be641fa39a06d994891aa))

## [0.1.20](https://github.com/Endika/flipper-wifi-census/compare/v0.1.19...v0.1.20) (2026-09-20)


### Bug Fixes

* reject captures over the 100-device cap with a clear message instead of risking OOM ([18206a7](https://github.com/Endika/flipper-wifi-census/commit/18206a79ffa91781916b41bbb5cc87a511f3a4b0))

## [0.1.19](https://github.com/Endika/flipper-wifi-census/compare/v0.1.18...v0.1.19) (2026-09-20)


### Bug Fixes

* read captures with more devices than the live cap, with a clear error otherwise ([a4a706b](https://github.com/Endika/flipper-wifi-census/commit/a4a706b370dc9ec1179e93256ff305b8cce87a9d))

## [0.1.18](https://github.com/Endika/flipper-wifi-census/compare/v0.1.17...v0.1.18) (2026-09-20)


### Features

* device detail, known-network tagging and rename/delete of known entries ([49f16db](https://github.com/Endika/flipper-wifi-census/commit/49f16db9afcff3beb522a9190712dc45ace91fe8))


### Performance Improvements

* lower the per-scan device cap to 100 for RAM headroom ([a11ba47](https://github.com/Endika/flipper-wifi-census/commit/a11ba4777965757fb35880c6bbb1d579f797992a))

## [0.1.17](https://github.com/Endika/flipper-wifi-census/compare/v0.1.16...v0.1.17) (2026-09-20)


### Features

* expand vendor OUI table from the IEEE registry (619 verified entries) ([fcbbbe2](https://github.com/Endika/flipper-wifi-census/commit/fcbbbe2c7bdb20c300a8095743626660e8b1ad9c))

## [0.1.16](https://github.com/Endika/flipper-wifi-census/compare/v0.1.15...v0.1.16) (2026-09-19)


### Bug Fixes

* parse Marauder v1.17 probe lines (bare RSSI, Ch, Client, Requesting SSID) ([776dc4a](https://github.com/Endika/flipper-wifi-census/commit/776dc4a16eeb2cd7fbc1a25d170176c655c96909))

## [0.1.15](https://github.com/Endika/flipper-wifi-census/compare/v0.1.14...v0.1.15) (2026-09-19)


### Features

* add a Serial debug view showing raw Marauder lines ([54cf475](https://github.com/Endika/flipper-wifi-census/commit/54cf47560dce5aaab886fc1be80114ae89ed3924))

## [0.1.14](https://github.com/Endika/flipper-wifi-census/compare/v0.1.13...v0.1.14) (2026-09-19)


### Bug Fixes

* also read a quoted SSID from Marauder probe lines lacking an SSID label ([40b674e](https://github.com/Endika/flipper-wifi-census/commit/40b674e1547f1fa1e2209f5fca46e13b2a5ef511))

## [0.1.13](https://github.com/Endika/flipper-wifi-census/compare/v0.1.12...v0.1.13) (2026-09-19)


### Features

* auto-save rotation option to capture huge venues across files ([f9c482e](https://github.com/Endika/flipper-wifi-census/commit/f9c482e11202a6cbab7df0e3942ac544489698b7))

## [0.1.12](https://github.com/Endika/flipper-wifi-census/compare/v0.1.11...v0.1.12) (2026-09-19)


### Features

* merge many captures off-device with the wc_merge host tool (no device cap) ([2f200ce](https://github.com/Endika/flipper-wifi-census/commit/2f200ce66162688a95bee4c76861defb98cc69f1))

## [0.1.11](https://github.com/Endika/flipper-wifi-census/compare/v0.1.10...v0.1.11) (2026-09-19)


### Features

* raise census ceiling to 320 devices for denser venues ([b0a8f67](https://github.com/Endika/flipper-wifi-census/commit/b0a8f671ede5c3ebec670b839c1545b8346610f3))

## [0.1.10](https://github.com/Endika/flipper-wifi-census/compare/v0.1.9...v0.1.10) (2026-09-19)


### Bug Fixes

* reserve census up front and shrink per-device size to stop mid-scan OOM reboots ([1ab1c42](https://github.com/Endika/flipper-wifi-census/commit/1ab1c42ad609c79b8978472d11cee15642005894))

## [0.1.9](https://github.com/Endika/flipper-wifi-census/compare/v0.1.8...v0.1.9) (2026-09-19)


### Features

* default merges to merge_* and show dropped count when the census caps ([4df77a3](https://github.com/Endika/flipper-wifi-census/commit/4df77a30123e178ddfdb2082771cd8fca90054e5))

## [0.1.8](https://github.com/Endika/flipper-wifi-census/compare/v0.1.7...v0.1.8) (2026-09-19)


### Bug Fixes

* stream capture save to avoid the double-buffer OOM (merge/scan reboot) ([b634ff4](https://github.com/Endika/flipper-wifi-census/commit/b634ff48e5178ba8693438c9f2e1c8fbb6ccb6ef))

## [0.1.7](https://github.com/Endika/flipper-wifi-census/compare/v0.1.6...v0.1.7) (2026-09-19)


### Bug Fixes

* census grows on demand (fits big scans/merges) and merge shows a result summary ([a2eb175](https://github.com/Endika/flipper-wifi-census/commit/a2eb175d408d6eed650043860d69ba460bfe836e))

## [0.1.6](https://github.com/Endika/flipper-wifi-census/compare/v0.1.5...v0.1.6) (2026-09-19)


### Bug Fixes

* merge dedups by exact MAC first, matching live-session behavior ([5acde18](https://github.com/Endika/flipper-wifi-census/commit/5acde18e3df0985a0df492712f0b3289706bac6b))
* shrink census memory so merge/compare/import fit the FAP heap ([a1d897b](https://github.com/Endika/flipper-wifi-census/commit/a1d897bb90b138f582e53aea1adf57980eb412b0))

## [0.1.5](https://github.com/Endika/flipper-wifi-census/compare/v0.1.4...v0.1.5) (2026-09-19)


### Features

* store IE fingerprint and detect vendor from probe vendor IEs ([ca0b9e8](https://github.com/Endika/flipper-wifi-census/commit/ca0b9e84c386dcd111c6fce9803812a1acf5e3d8))

## [0.1.4](https://github.com/Endika/flipper-wifi-census/compare/v0.1.3...v0.1.4) (2026-09-19)


### Features

* import a pcap into a census on-device and via a host tool ([4213ec1](https://github.com/Endika/flipper-wifi-census/commit/4213ec16caee6aff6ab897210153fbaec14c448c))
* parse raw 802.11 probe frames with an IE fingerprint ([e3e87d6](https://github.com/Endika/flipper-wifi-census/commit/e3e87d6f8cb63a50e51dad31152d3e838b4d75fe))

## [0.1.3](https://github.com/Endika/flipper-wifi-census/compare/v0.1.2...v0.1.3) (2026-09-19)


### Features

* label stable-MAC devices by vendor via a verified OUI table ([df34605](https://github.com/Endika/flipper-wifi-census/commit/df3460577050d46df6fd6b446f894122e13d252b))
* merge two captures into an accumulated census ([3ff7ac1](https://github.com/Endika/flipper-wifi-census/commit/3ff7ac1357c47c3dff8edecc4cdf08a4d9807c0c))
* prefill a timestamped default name when saving a capture ([2e5803d](https://github.com/Endika/flipper-wifi-census/commit/2e5803d7659e7e02abcf8c720591fb44cf41c47b))
* surface networks devices probe for and drop the AP counter ([9768291](https://github.com/Endika/flipper-wifi-census/commit/9768291741b47447c954079407a9059f66d64fa1))

## [0.1.2](https://github.com/Endika/flipper-wifi-census/compare/v0.1.1...v0.1.2) (2026-09-19)


### Features

* show version, author and GitHub link in About ([914cc30](https://github.com/Endika/flipper-wifi-census/commit/914cc303531ff91dc99748765637fd7275ef3af7))

## [0.1.1](https://github.com/Endika/flipper-wifi-census/compare/v0.1.0...v0.1.1) (2026-09-19)


### Features

* add binary capture codec and CSV export ([170186a](https://github.com/Endika/flipper-wifi-census/commit/170186aafcb743745210fd39c066f9e3befe0128))
* add capture comparison and known-device registry ([4ee4c15](https://github.com/Endika/flipper-wifi-census/commit/4ee4c1533d82759368b0b412095666eb8cfd5cae))
* add device observation and signature domain model ([16dabcd](https://github.com/Endika/flipper-wifi-census/commit/16dabcd00d966d0d8a218398e156e1b969c2754e))
* add furi serial, storage and clock adapters ([d3f1509](https://github.com/Endika/flipper-wifi-census/commit/d3f150900d5b52ec0b475a195dfdf83fac1315fd))
* add scan, capture, compare and known application services ([09b147c](https://github.com/Endika/flipper-wifi-census/commit/09b147ccc4f3c1ea8cedb2017674069c4d2b03fb))
* add scene-manager UI with scan, files, compare, known and settings ([6ae4bc1](https://github.com/Endika/flipper-wifi-census/commit/6ae4bc1f5891528e0180f4072bb437b0ca7bdd95))
* deduplicate observations into a unique-device census ([69531bc](https://github.com/Endika/flipper-wifi-census/commit/69531bc3cdda277a8e2a7b5574bd4ef85dd9d677))
* parse Marauder probe-sniff serial lines into observations ([643b56f](https://github.com/Endika/flipper-wifi-census/commit/643b56f23106cf238483e3dca3b66a0e512d2087))


### Bug Fixes

* bound Marauder RSSI/channel digits to avoid integer overflow ([c1924c4](https://github.com/Endika/flipper-wifi-census/commit/c1924c4163515ce65e5d733faf599db9e29756bd))
* heap-allocate known-db buffer to avoid FAP stack overflow ([928ec01](https://github.com/Endika/flipper-wifi-census/commit/928ec013d3e8ba2d6381f12c52d6a3329bd18cb9))
