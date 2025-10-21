#pragma once

/* THIS MUST BE FIRST so Protocol/Payload are known everywhere */
#include "_base.h"

/* Now include per-protocol public headers */
#include "continuity.h"
#include "easysetup.h"
#include "fastpair.h"
#include "lovespouse.h"
#include "nameflood.h"
#include "swiftpair.h"
#include "magicband.h"

/* Registry of all protocols */
extern const Protocol* protocols[];

/* Example composite that other code uses */
typedef struct {
    const Protocol* protocol;
    Payload         payload;
} Attack;