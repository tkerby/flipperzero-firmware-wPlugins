#pragma once
#include <toolbox/protocols/protocol.h>
#include "../tools/t5577.h"

typedef enum {
    LFRFIDFeatureASK = 1 << 0, /** ASK Demodulation */
    LFRFIDFeaturePSK = 1 << 1, /** PSK Demodulation */
    LFRFIDFeatureRTF = 1 << 2, /** Reader Talks First: ASK Demodulation with 2 way communication */
} LFRFIDFeature;

typedef enum {
    LFRFIDProtocolEM4100,
    LFRFIDProtocolEM4100_32,
    LFRFIDProtocolEM4100_16,
    LFRFIDProtocolElectra,
    LFRFIDProtocolH10301,
    LFRFIDProtocolIdteck,
    LFRFIDProtocolIndala26,
    LFRFIDProtocolIndala224,
    LFRFIDProtocolIOProxXSF,
    LFRFIDProtocolAwid,
    LFRFIDProtocolFDXA,
    LFRFIDProtocolFDXB,
    LFRFIDProtocolHidGeneric,
    LFRFIDProtocolHidExGeneric,
    LFRFIDProtocolPyramid,
    LFRFIDProtocolViking,
    LFRFIDProtocolJablotron,
    LFRFIDProtocolParadox,
    LFRFIDProtocolPACStanley,
    LFRFIDProtocolKeri,
    LFRFIDProtocolGallagher,
    LFRFIDProtocolHitag1,
    LFRFIDProtocolNexwatch,
    LFRFIDProtocolInstafob,
    LFRFIDProtocolSecurakey,
    LFRFIDProtocolGProxII,
    LFRFIDProtocolMax,
} LFRFIDProtocol;

extern const ProtocolBase* lfrfid_protocols[];

typedef enum {
    LFRFIDWriteTypeT5577,
} LFRFIDWriteType;

typedef struct {
    LFRFIDWriteType write_type;
    union {
        LFRFIDT5577 t5577;
    };
} LFRFIDWriteRequest;
