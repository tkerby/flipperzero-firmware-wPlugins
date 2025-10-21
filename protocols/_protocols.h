#pragma once
#include "_base.h"

// Pull in each protocol’s header (each one should also include _base.h itself)
#include "continuity.h"
#include "easysetup.h"
#include "fastpair.h"
#include "lovespouse.h"
#include "nameflood.h"
#include "swiftpair.h"
#include "magicband.h"

// Master list (defined in _protocols.c)
extern const Protocol* protocols[];

// Handy pair for menus/selectors
typedef struct {
    const Protocol* protocol;
    Payload         payload;
} ProtocolWithPayload;