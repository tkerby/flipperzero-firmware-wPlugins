#pragma once

#include <notification/notification.h>

#include "game.h"

extern const NotificationSequence sequence_earn_point;

void game_notify(GameContext* game_context, const NotificationSequence* sequence);
