#ifndef TRADE_H
#define TRADE_H

#pragma once

#include <gui/view.h>
#include "../pokemon_data.h"

void* trade_alloc(
    PokemonData* pdata,
    struct gblink_pins* gblink_pins,
    ViewDispatcher* view_dispatcher,
    uint32_t view_id);

void trade_free(ViewDispatcher* view_dispatcher, uint32_t view_id, void* trade_ctx);

#endif /* TRADE_H */
