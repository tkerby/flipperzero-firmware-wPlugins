### New changes 
* If you have copied any apps manually into `apps` folder - remove `apps` folder or that specific apps you copied on your microSD before installing this release to avoid issues!
* Plugins: RFID and iButton Fuzzer remove excessive free's (thanks @Willy-JL)
* Plugins: Use COUNT_OF in mouse jiggler
* Plugins: Added Numpad keyboard to HID app (by @clipboard1 | PR #452)
* About: Don't show 00 on about screens
* SubGHz: Combine FuriString allocs and other small changes
* Infrared: Updated universal remote assets (by @amec0e | PR #454)
* Update slideshow: Replace QR code with good old link
* OFW: Various Furi/FuriHal bug fixes and improvements -> **Breaking API change, api was changed from 24.x to 26.x** 
* OFW: Loader refactoring, part 1 -> **Breaking API change, api was changed from 23.x to 24.x** **(this will make your manually copied plugins not work, update them in same way you installed them, or delete `apps` folder and then install firmware, if you using extra pack builds (with `e` in version) all apps in _Extra will be updated automatically)**
* OFW: Dolphin builder in ufbt; minor ufbt/fbt improvements
* OFW: Added API version to device info 
* OFW: Gui: relax some asserts in view
* OFW: Move gauge calibration to separate header, add f18 calibration
* OFW: Fix TERMINFO on Linux systems 

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


