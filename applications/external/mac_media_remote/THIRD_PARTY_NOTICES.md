# Third-party notices

- `flipper/now_playing` adapts [villenull/flipper-now-playing](https://github.com/villenull/flipper-now-playing), licensed GPL-3.0. The original copyright belongs to its contributors. Changes here add a Mac companion and protocol application version 3 for Cyrillic UTF-8.
- `flipper/now_playing/ui/np_cyrillic_font.c` embeds the public-domain `u8g2_font_6x12_t_cyrillic` font from official Flipper Zero firmware 1.4.3 (`lib/u8g2/u8g2_fonts.c`); its source header calls it a public-domain terminal emulator font.
- The Flipper build uses official [flipperdevices/flipperzero-firmware](https://github.com/flipperdevices/flipperzero-firmware) sources under GPL-3.0. Firmware and its toolchain are build dependencies and are not part of this repository.
- The Mac companion invokes [nowplaying-cli](https://github.com/kirtan-shah/nowplaying-cli) 2.1.0, installed separately through Homebrew. It is GPL-3.0-or-later and uses the private macOS MediaRemote framework. Its binary is not bundled here.

This repository is distributed under GPL-3.0; see [LICENSE](LICENSE).
