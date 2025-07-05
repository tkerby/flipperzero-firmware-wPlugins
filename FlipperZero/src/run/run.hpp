#pragma once
#include "easy_flipper/easy_flipper.h"
#include "engine/engine.hpp"

class FlipWorldApp;

class FlipWorldRun
{
    bool shouldReturnToMenu = false; // Flag to signal return to menu
public:
    FlipWorldRun();
    ~FlipWorldRun();
    //
    bool isActive() const { return shouldReturnToMenu == false; } // Check if the run is active
    void updateDraw(Canvas *canvas);                              // update and draw the run
    void updateInput(InputEvent *event);                          // update input for the run
};
