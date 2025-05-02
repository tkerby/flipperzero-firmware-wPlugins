#pragma once
#include "Arduino.h"
#include "vector.h"

namespace VGMGameEngine
{

#define BUTTON_UP 0
#define BUTTON_DOWN 1
#define BUTTON_LEFT 2
#define BUTTON_RIGHT 3
#define BUTTON_CENTER 4
#define BUTTON_BACK 5
#define BUTTON_START 6

    /*
    # Wiring (HW504 -> VGM):
    # SW -> GP21
    # VRx -> GP27
    # VRy -> GP26
    # GND -> GND
    # 5V -> 3v3
    */

#define HW_LEFT_BUTTON 0
#define HW_RIGHT_BUTTON 1
#define HW_UP_BUTTON 2
#define HW_DOWN_BUTTON 3
#define HW_CENTER_BUTTON 4

#define HW_ORIENTATION_NORMAL 0
#define HW_ORIENTATION_90 1
#define HW_ORIENTATION_180 2
#define HW_ORIENTATION_270 3

    class HW504
    {
    public:
        int x_axis;
        int y_axis;
        int orientation;
        int button;

        HW504(int x_pin = 26, int y_pin = 27, int button_pin = 21, int orientation = HW_ORIENTATION_NORMAL);
        Vector axes();                             // Read the raw ADC values from both axes and transform them based on the current orientation.
        bool value(int button = HW_CENTER_BUTTON); // Return the state of a button based on the transformed x,y axis values.

    private:
        int _button(); // Read the button value from the joystick
        int _x_axis(); // Return the transformed x-axis value
        int _y_axis(); // Return the transformed y-axis value
    };

    class ButtonUART
    {
    public:
        ButtonUART();
        void run();
        int last_button;

    private:
        SerialPIO *serial;
    };

    class Input
    {
    public:
        int pin;
        int button;
        float elapsed_time;
        bool was_pressed;
        HW504 *hw;
        ButtonUART *bt;
        Input();
        Input(int pin, int button);
        Input(HW504 *hw, int button);
        Input(ButtonUART *bt);
        bool is_pressed();
        bool is_held(int duration = 3);
        void run();

        operator bool() const;
    };

}