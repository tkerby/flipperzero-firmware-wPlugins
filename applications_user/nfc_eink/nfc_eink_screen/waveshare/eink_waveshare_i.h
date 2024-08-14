#pragma once
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_listener.h>

typedef enum {
    NfcEinkScreenTypeWaveshareUnknown,
    NfcEinkScreenTypeWaveshare2n13inch,
    NfcEinkScreenTypeWaveshare2n9inch,
    //NfcEinkTypeWaveshare4n2inch,

    NfcEinkScreenTypeWaveshareNum
} NfcEinkScreenTypeWaveshare;
