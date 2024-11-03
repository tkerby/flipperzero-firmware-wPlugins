#pragma once
#include <engine/engine.h>
#include <engine/entity.h>
#include "game.h"

extern bool firstLevelCompleted;
extern bool startedGame;
extern bool hasSpawnedFirstMob;
void tutorial_update(Entity* self, GameManager* manager, void* context, Vector* pos);

void tutorial_render(GameManager* manager, Canvas* canvas, void* context);
