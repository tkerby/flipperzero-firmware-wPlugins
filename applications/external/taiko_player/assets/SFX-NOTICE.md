# Drum sound attribution

The bundled dong.ogg and ka.ogg, and the WAV copies generated from them, come from
[TJAPlayer3](https://github.com/AioiLight/TJAPlayer3)'s default SimpleStyle skin.
This notice and the accompanying license apply to these two sounds, not the app icon or logo.

- Upstream commit: [59835a522887c67b8db0e60d89a1e61ed3220742](https://github.com/AioiLight/TJAPlayer3/tree/59835a522887c67b8db0e60d89a1e61ed3220742)
- Original directory: Test/System/SimpleStyle/Sounds/Taiko/
- Upstream [redistribution statement](https://github.com/AioiLight/TJAPlayer3/blob/59835a522887c67b8db0e60d89a1e61ed3220742/Readme.txt): the program and default skin are provided under the MIT license.
- Copyright notice: Copyright (c) 2017 J.MIR
- License: [MIT license text, preserved from upstream](SFX-LICENSE.txt)

## Files and verification

- [dong.ogg](https://raw.githubusercontent.com/AioiLight/TJAPlayer3/59835a522887c67b8db0e60d89a1e61ed3220742/Test/System/SimpleStyle/Sounds/Taiko/dong.ogg): SHA-256 dc0a84a04faf878412e4c0f1b157b7284ab0f9276029be94c1cb9c5c2928d875
- [ka.ogg](https://raw.githubusercontent.com/AioiLight/TJAPlayer3/59835a522887c67b8db0e60d89a1e61ed3220742/Test/System/SimpleStyle/Sounds/Taiko/ka.ogg): SHA-256 867ed6f937342538ed54587faf232f093d43a978a499f4196936bd5c0d2a185e

The OGG files are copied without modification. scripts/prepare_sfx.py converts them
to 22050 Hz, unsigned 8-bit mono PCM WAV, removes audio metadata, and performs no
additional gain adjustment or trimming. The converted files retain this attribution
and the upstream MIT license. Keep SFX-LICENSE.txt and SFX-NOTICE.md with redistributed
copies of the sound assets.
