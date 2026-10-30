#include "onkyo_commands.h"

/**
 * Captured from the original Onkyo remote with the Flipper's learn mode; the
 * raw capture is res/Onkyo.ir. `.ir` files store both fields least-significant
 * byte first, so `command: 05 FA 00 00` becomes 0xFA05 here.
 *
 * Every address ends in 0xD2 -- the Onkyo customer code -- with the high byte
 * selecting a device group:
 *
 *   0x03D2  main receiver  mute, volume, flat, HDMI, AUX, RCA, OPT
 *   0x04D2  system         power
 *   0x09D2  transport      previous, next
 *   0x1AD2  player         play
 *   0x1ED2  network/USB    USB, Bluetooth
 *
 * Captured from an Onkyo GX-30ARC's own remote and tested against that system.
 * Onkyo command bytes are model-specific and none of this has been tried on
 * another model: the addresses, the device grouping and the command bytes may
 * all differ on one.
 *
 * USB and Bluetooth are the odd pair. Every other command's high byte is the
 * bitwise complement of its low byte (0xCB -> 0x34), but those two share the
 * low byte 0x51 and carry 0x0F / 0x0E where 0xAE would be expected. NECext does
 * not enforce the complement, so they go out exactly as captured.
 */
#define ONKYO_CMD(name, address, command) {(name), InfraredProtocolNECext, (address), (command)}

const OnkyoCommand onkyo_commands[OnkyoCmdCount] = {
    [OnkyoCmdPower] = ONKYO_CMD("Power", 0x04D2, 0x34CB),
    [OnkyoCmdMute] = ONKYO_CMD("Mute", 0x03D2, 0xFA05),
    [OnkyoCmdPlay] = ONKYO_CMD("Play/Pause", 0x1AD2, 0xA35C),
    [OnkyoCmdVolDown] = ONKYO_CMD("Volume Down", 0x03D2, 0xFC03),
    [OnkyoCmdVolUp] = ONKYO_CMD("Volume Up", 0x03D2, 0xFD02),
    [OnkyoCmdPrev] = ONKYO_CMD("Previous", 0x09D2, 0x6F90),
    [OnkyoCmdNext] = ONKYO_CMD("Next", 0x09D2, 0x708F),
    [OnkyoCmdFlat] = ONKYO_CMD("Flat", 0x03D2, 0xAB54),
    [OnkyoCmdInputUsb] = ONKYO_CMD("USB", 0x1ED2, 0x0F51),
    [OnkyoCmdInputBluetooth] = ONKYO_CMD("Bluetooth", 0x1ED2, 0x0E51),
    [OnkyoCmdInputHdmi] = ONKYO_CMD("HDMI", 0x03D2, 0xB748),
    [OnkyoCmdInputAux] = ONKYO_CMD("AUX", 0x03D2, 0xF906),
    [OnkyoCmdInputRca] = ONKYO_CMD("RCA", 0x03D2, 0xEE11),
    [OnkyoCmdInputOpt] = ONKYO_CMD("OPT", 0x03D2, 0xA857),
};
