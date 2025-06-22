#pragma once
#include <engine/draw.hpp>

class Loading
{
public:
    Loading(Draw *draw);
    //
    void animate();
    void stop();
    //
    void setText(const char *text) { currentText = text; }
    //
    uint16_t getTimeElapsed() { return timeElapsed; }

private:
    Draw *draw = nullptr;
    void drawSpinner();
    uint16_t spinnerPosition;
    uint16_t timeElapsed;
    uint16_t timeStart;
    bool animating = false;
    const char *currentText = "Loading...";
};
