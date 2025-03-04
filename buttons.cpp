#ifndef _BUTTONS_CONTROL_CLASS_
#define _BUTTONS_CONTROL_CLASS_

#include <furi.h>

class Buttons {
private:
    int count;
    int* states;
    unsigned long lastPress;
    unsigned long pressInterval;
    void (*handler)(int);

    int GetButton(int number) {
        return 2 + number;
    }

    bool canPress() {
        return false;
    }

public:
    Buttons() {
        lastPress = 0;
        pressInterval = 100;
        count = 2;
        states = new int[count];
    }

    void Init(void (*pressedHandler)(int)) {
        UNUSED(pressedHandler);
    }

    void CheckButtons() {
        FURI_LOG_I("TEST", "I PRESS BUTTON");
        furi_delay_ms(5000);
    }
};

#endif //_BUTTONS_CONTROL_CLASS_