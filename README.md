# Flipper Zero I2C Explorer
## Introduction

This is a simple tool for interactive exploration of an I2C bus
topology and modification of device register states. 

Connect the following pins to the I2C bus. Note that pull-up resistors
from SCL to VCC and from SDA to VCC are necessary as part of the
standard bus configuration. Use only 3.3V logic levels.

 - 3V3 (3V3, pin 9)  = VCC
 - GND (GND, pin 18) = GND
 - SCL (C0, pin 16)  = SCL
 - SDA (C1, pin 15)  = SDA

If the app does not detect active SCL or SDA lines (i.e. they are not
connected or the pull-ups are not present), a confused dolphin face
will stare at you, and !SCL and/or !SDA will appear at the bottom of
the app indicating which line(s) are not connected.

When successful, a list of detected device addresses will appear under
"I2C Devices:". Scroll down to select one, and a live register dump
will appear under "Regs:". Scroll right to highlight the registers
column, where you can scroll up and down through the entire 8-bit
register space. Scroll right again to access the bitfield column,
where you can flip individual bits (use Up/Down to navigate among bits
and OK to flip them). Scroll right one last time to access the
load/save menu options, which will dump or restore the selected
device's entire register space to a file. (Note that not all devices
will correctly reproduce their prior state from such a dump; behavior
is manufacturer-specific as to whether a device will accept verbatim
writes or have more complex cross-register side effects. You can,
however, manually edit a dump to include only the specific registers
you wish to write back, in the order you wish to write them.)

## Installation Directions

If building from source, clone repo, run `ufbt` from the
`i2c_explorer` directory to build, and then copy
`dist/i2c_explorer.fap` to `/ext/apps/GPIO/` on the Flipper device
using File Manager in the Flipper app (may be under "Experimental
Options") or with a tool such as qFlipper.

## Launching the app

These directions assume you are starting at the Flipper desktop.  If
not, please press the back button until you are at the desktop.

- Press the OK button on the flipper to pull up the main menu.
- Choose "Apps" from the menu.
- Choose "GPIO" from the sub-menu.
- Choose "I2C Explorer".
- Press DOWN to begin selecting a device, and press RIGHT to move to other columns.
- Press the BACK button to exit.

## I2C dump file format

`.i2c` files consist of a series of lines of the form `RR: VV` where
RR is a 2-digit hex register address and VV is a 2-digit hex
value. (They must be padded to 2 digits even for numbers less than
0x0F.) By default, dump files do not include the device address and,
when loaded, will write their contents to whatever I2C device is
currently selected in the GUI. However, manually created `.i2c` files
can include `Address: XX`, where XX is a 2-digit hex address. All
subsequent register writes will be directed to that device
address. This can be repeated multiple times in a file, allowing for
complex, multi-device configuration replay sequences. Lines beginning
with a `#` are ignored as comments.

## Credits

Developed by @4mb3rz over a sleepy weekend as a first Flipper Zero
app, because writing new firmware is annoying when you haven't even
finished the hardware design, and wouldn't it be great if you could
test I2C devices interactively?

Who is @amb3rz anyway, and why did they snipe my username?
Grumble. I'm the real Amber! :P

Originally based on some sample code by @MrDerekJamison -- Thanks!

