#pragma once

#include <infrared.h>

/**
 * Every command the remote can send.
 *
 * `onkyo_commands` is indexed by this enum -- the table uses designated
 * initialisers so the two cannot drift apart.
 */
typedef enum {
    OnkyoCmdPower,
    OnkyoCmdMute,
    OnkyoCmdPlay,
    OnkyoCmdVolDown,
    OnkyoCmdVolUp,
    OnkyoCmdPrev,
    OnkyoCmdNext,
    OnkyoCmdFlat,
    OnkyoCmdInputUsb,
    OnkyoCmdInputBluetooth,
    OnkyoCmdInputHdmi,
    OnkyoCmdInputAux,
    OnkyoCmdInputRca,
    OnkyoCmdInputOpt,
    OnkyoCmdCount,
} OnkyoCmdId;

/**
 * A single IR command as captured from the original remote.
 *
 * Every entry is NECext: a 16-bit address and a 16-bit command, both
 * model-specific. See docs/SPECIFICATION.md section 4 for the capture and what
 * about it is still unverified.
 */
typedef struct {
    const char* name;
    InfraredProtocol protocol;
    uint32_t address;
    uint32_t command;
} OnkyoCommand;

extern const OnkyoCommand onkyo_commands[OnkyoCmdCount];
