# TonUINO RFID Card Writer for Flipper Zero

A Flipper Zero application to write RFID cards for TonUINO music boxes.

## Overview

This app allows you to create and write TonUINO-compatible RFID cards directly from your Flipper Zero device. TonUINO cards use a specific 16-byte format to control playback modes and folder selection.

## Card Format

Each TonUINO card contains 16 bytes:

- **Bytes 0-3**: Box identifier (always `13 37 B3 47`)
- **Byte 4**: Version (currently `0x02`)
- **Byte 5**: Folder number (1-99, hex `0x01`-`0x63`)
- **Byte 6**: Playback mode (see modes below)
- **Byte 7**: Special value 1 (for von-bis modes)
- **Byte 8**: Special value 2 (for von-bis modes)
- **Bytes 9-15**: Reserved (set to `0x00`)

## Playback Modes

1. **Hoerspiel** (Random) - Plays a random file from the folder
2. **Album** (Complete) - Plays the entire folder in order
3. **Party** (Shuffle) - Plays files in random order
4. **Einzel** (Single) - Plays a specific file from the folder
5. **Hoerbuch** (Progress) - Plays folder and remembers progress
6. **Admin** - Admin functions
7. **Hoerspiel Von-Bis** - Random file from a range (uses Special values)
8. **Album Von-Bis** - All files from a range (uses Special values)
9. **Party Von-Bis** - Shuffled files from a range (uses Special values)
10. **Hoerbuch Einzel** - Single track with progress tracking
11. **Repeat Last** - Repeats the last card/shortcut

## Building

1. Clone the Flipper Zero firmware repository:
   ```bash
   git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
   cd flipperzero-firmware
   ```

2. Copy this application to `applications_user/tonuino_writer/`:
   ```bash
   cp -r /path/to/tonuino-flipper-zero/* applications_user/tonuino_writer/
   ```

3. Build the application:
   ```bash
   ./fbt fap_tonuino_writer
   ```

4. Install to your Flipper Zero:
   ```bash
   ./fbt launch_app APPSRC=tonuino_writer
   ```

## Usage

1. Launch the app from the Applications menu on your Flipper Zero
2. Use the menu to configure:
   - **Set Folder**: Choose folder number (1-99)
   - **Set Mode**: Select playback mode
   - **Set Special 1**: Set first special value (0-255, for von-bis modes)
   - **Set Special 2**: Set second special value (0-255, for von-bis modes)
   - **View Card**: Preview the card configuration
   - **Write Card**: Write the card to an RFID tag

3. When writing:
   - Place a compatible RFID card (MIFARE Classic) on the back of your Flipper Zero
   - Wait for the write operation to complete
   - You'll see a success or error message

## Special Values (Von-Bis Modes)

For modes 7-9 (Von-Bis modes), the Special values define the track range:
- **Special 1**: Start track number
- **Special 2**: End track number

Example: To play tracks 5-10 in Album Von-Bis mode, set Special 1 to 5 and Special 2 to 10.

## Notes

- The app writes to block 0 of MIFARE Classic cards
- Ensure your RFID cards are writable (not locked)
- The box ID `13 37 B3 47` is standard for TonUINO devices
- Folder numbers must be between 1 and 99

## References

- [TonUINO RFID Card Format Discussion](https://discourse.voss.earth/t/format-der-rfid-karten/13284)
- [Flipper Zero Developer Documentation](https://developer.flipper.net/)

## License

This application is provided as-is for use with Flipper Zero and TonUINO devices.

