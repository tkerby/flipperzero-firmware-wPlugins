### New changes
* **Warning! After installing, Desktop settings (Favoutite apps, PIN Code, AutoLock time..) will be resetted to default due to OFW changes, Please set your PIN code, Favourite apps again in Settings->Desktop**
* New way of changing device name -> **Now can be changed in Settings->Desktop** (by @xMasterX and @Willy-JL)
* Plugins: BadBT plugin (BT version of BadKB) [(by Willy-JL, ClaraCrazy, XFW contributors)](https://github.com/ClaraCrazy/Flipper-Xtreme/tree/dev/applications/main/bad_kb)
* Plugins: WiFi Marauder -> Added sniff pmkid on selected aps from 0.10.4 update (by @clipboard1)
* Plugins: SubGHz Bruteforcer -> Increase delay just a little bit to fix some cases when receiver will not get codes and decrease manual transmit delay
* Plugins: UART Terminal -> Fix crashes on plugin load with RX connected
* NFC: Mifare mini with SAK 0x89 support
* SubGHz: **CAME Atomo - Add manually support and custom buttons support**
* SubGHz: Fix crashes when deleting signals using right arrow button in `Read` mode
* SubGHz: Restore Rx indication after deletion after Memory is FULL (by @wosk | PR #464)
* SubGHz: **App refactoring** (OFW code ported + our code was refactored/cleaned up too) (by @gid9798 and @xMasterX | PR #461)
* SubGHz: Using scene manager functions in DecodeRAW (by @gid9798 | PR #462)
* SubGHz: Protocols and custom buttons refactoring (by @gid9798 | PR #465)
* SubGHz: Move `counter increase` setting out of debug, change max value
* GUI: Submenu locked elements (by @Willy-JL and @giacomoferretti)
* GUI: Text Input improvements, added cursor and ability to set minimal length (by @Willy-JL)
* BT API: Functions that allow to change bt mac address and device broadcasted name (by @Willy-JL and XFW contributors)
* Infrared: `External output` move out of debug and add power option for external modules
* Infrared: Updated universal remote assets (by @amec0e | PR #474)
* Extra pack: Some app fixes
* FBT: Fix vscode example config for debug builds - please run `./fbt vscode_dist` again if you had issues with debug builds size
* OFW PR 2316: NFC V support (by @g3gg0 & @nvx)
* OFW PR 2669: nfc: Fix MFUL tearing flags read (by @GMMan)
* OFW PR 2666: BadUSB: Add fr-FR-mac key layout (by @FelixLgr)
* OFW: api: added lib/nfc/protocols/nfc_util.h
* OFW: fix PIN retry count reset on reboot 
* OFW: fbt: allow strings for fap_version field in app manifests
* OFW: Rpc: add desktop service. Desktop: refactor locking routine. **Now PIN lock is actually cannot be bypassed by reboot!** / **Desktop settings will be reset, please set your PIN and favourite apps again!**
* OFW: Part 2 of hooking C2 IPC
* OFW: ble: attempt to handle hardfaulted c2
* OFW: Add Mfkey32 application
* OFW: Added DigitalSequence and PulseReader
* OFW: Debug: revert cortex debug to lxml and drop DWT **(reapply your VSCode launch.json from example folder)**
* OFW: furi_crash: added C2 status; added fw-version gdb command
* OFW: Removed STM32CubeWB module
* OFW: API version in UI
* OFW: ufbt: deploying sample ufbt automation for new apps; added `source "ufbt -s env"` for toolchain access
* OFW: Fix storage.py exist_dir logic

#### [🎲 Download latest extra apps pack](https://github.com/xMasterX/all-the-plugins/archive/refs/heads/main.zip)

[-> How to install firmware](https://github.com/DarkFlippers/unleashed-firmware/blob/dev/documentation/HowToInstall.md)

[-> Download qFlipper (official link)](https://flipperzero.one/update)

### Thanks to our sponsors:
callmezimbra, Quen0n, MERRON, grvpvl (lvpvrg), art_col, ThurstonWaffles, Moneron, UterGrooll, LUCFER, Northpirate, zloepuzo, T.Rat, Alexey B., ionelife, ...
and all other great people who supported our project and me (xMasterX), thanks to you all!

**Note: To avoid issues with .dfu, prefer installing using .tgz with qFlipper, web updater or by self update package, all needed assets will be installed**

**Recommended option - Web Updater**

What means `n` or `e` in - `flipper-z-f7-update-(version)(n / e).tgz` ? - `n` means this build comes without our custom animations, only official flipper animations, 
`e` means build has extra apps pack preinstalled

Self-update package (update from microSD) - `flipper-z-f7-update-(version).zip` or download `.tgz` for mobile app / qFlipper


