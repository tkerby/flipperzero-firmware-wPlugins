## Main changes
- Current API: 87.4
* SubGHz: **Cardin S449 protocol full support** (64bit keeloq) (with Add manually, and all button codes) (**use FSK12K modulation to read the remote**) (closes issues #735 #908) (by @xMasterX and @zero-mega (thanks!))
* SubGHz: **Beninca ARC AES128 protocol full support** (128bit dynamic) (with Add manually, and 3 button codes) (resolves issue #596) (by @xMasterX and @zero-mega)
* SubGHz: **Treadmill37 protocol support** (37bit static) (by @xMasterX)
* SubGHz: **Jarolift protocol full support** (72bit dynamic) (with Add manually, and all button codes) (by @xMasterX & d82k & Steffen (bastelbudenbuben de))
* SubGHz: **New modulation FSK with 12KHz deviation**
* SubGHz: **KingGates Stylo 4k - Add manually and button switch support** + refactoring of encoder
* SubGHz: **Stilmatic (R-Tech) - 12bit discr. fix & button 9 support** (two buttons hold simulation) (mapped on arrow keys)
* SubGHz: **Counter editor refactoring** (PR #939 | by @Dmitry422)
* SubGHz: **Alutech AT-4N & Nice Flor S turbo speedup** (PR #942 | by @Dmitry422)
* SubGHz: **Sommer fm2 in Add manually now uses FM12K modulation** (Sommer without fm2 tag uses FM476) (try this if regular option doesn't work for you)
* SubGHz: **Sommer - last button code 0x6 support** (mapped on arrow keys)
* SubGHz: **V2 Phoenix (Phox) added 2 counter modes support** (docs updated)
* SubGHz: Add 390MHz, 430.5MHz to default hopper list (6 elements like in OFW) (works well with Hopper RSSI level set for your enviroment)
* SubGHz: Fixed button mapping for **FAAC RC/XT**
* SubGHz: KeeLoq **display decrypted hop** in `Hop` instead of showing encrypted as is (encrypted non byte reversed hop is still displayed in `Key` field)
* SubGHz: **BFT KeeLoq** try decoding with **zero seed** too
* SubGHz: KeeLoq **display BFT programming mode TX** (when arrow button is held)
* NFC: Handle PPS request in ISO14443-4 layer (by @WillyJL)
* NFC: Fixes to `READ_MULTI` and `GET_BLOCK_SECURITY` commands in ISO 15693-3 emulation (by @WillyJL & @aaronjamt)
* Archive: Allow folders to be pinned (by @WillyJL)
* Apps: Build tag (**27jan2026**) - **Check out more Apps updates and fixes by following** [this link](https://github.com/xMasterX/all-the-plugins/commits/dev)
## Other changes
* UI: Various small changes
* Desktop: Disable winter holidays anims 
* OFW PR 4333: NFC: Fix sending 32+ byte ISO 15693-3 commands (by @WillyJL)
* NFC: Fix LED not blinking at SLIX unlock (closes issue #945)
* SubGHz: Improve docs on low level code (PR #949 | by @Dmitry422)
* SubGHz: Fix Alutech AT4N false positives
* SubGHz: Cleanup of extra local variables
* SubGHz: Replaced Cars ignore option with Revers RB2 protocol ignore option
* SubGHz: Moved Starline, ScherKhan, Kia decoders into external app
* SubGHz: Possible Sommer timings fix
* SubGHz: Various fixes
* SubGHz: Nice Flor S remove extra uint64 variable
* SubGHz: Rename Sommer(fsk476) to Sommer (Sommer keeloq works better with FM12K) + added backwards compatibility with older saved files
* Docs: Add full list of supported SubGHz protocols and their frequencies/modulations that can be used for reading remotes - [Docs Link](https://github.com/DarkFlippers/unleashed-firmware/blob/dev/documentation/SubGHzSupportedSystems.md)
* Desktop: Show debug status (D) if clock is enabled and debug flag is on (PR #942 | by @Dmitry422)
* NFC: Fix some typos in Type4Tag protocol (by @WillyJL)
* Clangd: Add clangd parameters in IDE agnostic config file (by @WillyJL)
<br><br>
#### Known NFC post-refactor regressions list: 
- Mifare Mini clones reading is broken (original mini working fine) (OFW)
- While reading some EMV capable cards via NFC->Read flipper may crash due to Desfire poller issue, read those cards via Extra actions->Read specific card type->EMV 

### THANKS TO ALL RM SPONSORS FOR BEING AWESOME!

# MOST OF ALL, THANK YOU TO THE FLIPPER ZERO COMMUNITY THAT KEEPS GROWING OUR PROJECT!
