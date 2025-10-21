// protocols/_protocols.h
#pragma once

// Must come first so Protocol/Payload are known to everything below
#include "_base.h"

#include "continuity.h"
#include "easysetup.h"
#include "fastpair.h"
#include "lovespouse.h"
#include "nameflood.h"
#include "swiftpair.h"
#include "magicband.h"

// (rest of your declarations)

// Master list (defined in _protocols.c)
extern const Protocol* protocols[];

// Handy pair for menus/selectors
typedef struct {
    const Protocol* protocol;
    Payload         payload;
} ProtocolWithPayload;