# Spectre

A ghost hunting kit: an EMF-style needle, a spirit box, a spirit voice and a radar, all driven by one shared spirit energy level. It is a toy: real inputs feed the level where the hardware has them, and the hardware random number generator fills in the rest.

## Tabs

- **SENSE:** a needle from QUIET to CONTACT. A nearby NFC or 125 kHz reader field pushes it hard, a drop in battery temperature since you started reads as a cold spot. Geiger crackle that quickens as the needle climbs
- **BOX:** a spirit box. Sweeps a Sub-GHz band, draws the noise floor and plays the channel hopping as static. When something real is on the air it locks on and a word comes through
- **VOICE:** words arrive on their own when the energy surges, spelled out letter by letter. Press OK to ask a question and wait for the answer. The last three words stay on screen
- **RADAR:** a sweeping scope with drifting echoes, the nearest one's range and bearing, and a ping as the beam crosses each. Press OK for a yes or no

Every surge flashes the screen, buzzes, and the LED breathes in the tab's colour.

## Controls

- Left / Right: switch tab
- Up / Down: BOX: change band
- OK: SENSE: reset baseline. BOX: hold the sweep. VOICE and RADAR: ask
- Hold OK: sound on or off
- Back: exit

## What it transmits

Nothing. Sub-GHz is receive-only, and SENSE uses the NFC and RFID front ends in their passive field-detect modes.
