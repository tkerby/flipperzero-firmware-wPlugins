# Tricorder

A handheld signal scanner styled after a certain starship's field instrument. Eight sensors, one sweep that runs them all.

## Sensors

- **RF:** Sub-GHz spectrum sweep across a band with peak hold and the strongest frequency, or a scrolling waterfall. Lock on to a frequency to chart its strength over time, hear it as Geiger clicks, and name any protocol the firmware's decoders recognise
- **IR:** the last infrared signal as a waveform, with the protocol, address and command when it decodes. The signal can be sent back out or saved to the SD card
- **EMF:** whether an NFC (13.56 MHz) or low-frequency RFID reader field is nearby, the LF field's frequency, and a short history
- **TAG:** which NFC protocols a tag held to the back answers to, and its UID
- **KEY:** reads 125 kHz RFID fobs and cards, or iButton keys
- **PIN:** voltage on a GPIO pin (0 to 2.5 V) as a number and a trace
- **PWR:** battery voltage, temperature, charge and current draw
- **ALL:** full sweep: runs RF, IR, EMF and TAG in turn with a progress bar, and ends each pass with a report card

Every new contact flashes the panel, buzzes and blinks the LED. The tricorder warbles on a contact, higher for stronger signals, and a dim LED breathes in the active sensor's colour. Sound and the LED can be switched off in the menu.

## Controls

- Up / Down: switch sensor
- Left / Right: RF scan: change band. RF track: tune (hold for bigger steps). IR: scroll the waveform. KEY: RFID or iButton. PIN: choose pin
- OK: RF: lock on to the peak, or return to the sweep. IR: zoom. Other tabs: clear
- Hold OK: menu: RF view and modulation, IR send and save, sound, LED
- Back: leave RF track or the menu, otherwise exit

Sound, the LED, the last tab and the other choices are remembered between runs.

## What it transmits

Sub-GHz is receive-only. The IR tab transmits only when you choose Send signal from the menu. The TAG and KEY tabs power the NFC and RFID coils to wake tags, as any reader does.

Saved infrared signals are appended to infrared/Tricorder.ir on the SD card, which opens in the Flipper's own Infrared app.

## The PIN probe

The probe measures 0 to 2.5 V against ground (pin 8, 11 or 18). Do not connect anything above 3.3 V to a GPIO pin.
