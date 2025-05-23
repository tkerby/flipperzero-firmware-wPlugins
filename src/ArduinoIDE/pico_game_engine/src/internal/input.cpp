#include "input.hpp"
#include <string.h>
namespace PicoGameEngine
{

    HW504::HW504(int x_pin, int y_pin, int button_pin, int orientation)
    {
        this->x_axis = x_pin;
        this->y_axis = y_pin;
        this->button = button_pin;
        this->orientation = orientation;
        pinMode(this->x_axis, INPUT);
        pinMode(this->y_axis, INPUT);
        pinMode(this->button, INPUT_PULLUP);
    }

    Vector HW504::axes()
    {
        Vector v;
        v.x = analogRead(this->x_axis);
        v.y = analogRead(this->y_axis);

        switch (this->orientation)
        {
        case HW_ORIENTATION_NORMAL:
            break;
        case HW_ORIENTATION_90:
            v = Vector(v.y, 1023 - v.x);
            break;
        case HW_ORIENTATION_180:
            v = Vector(1023 - v.x, 1023 - v.y);
            break;
        case HW_ORIENTATION_270:
            v = Vector(1023 - v.y, v.x);
            break;
        default:
            break;
        }
        return v;
    }

    int HW504::_button()
    {
        return digitalRead(this->button);
    }

    int HW504::_x_axis()
    {
        Vector v = this->axes();
        return v.x;
    }

    int HW504::_y_axis()
    {
        Vector v = this->axes();
        return v.y;
    }

    bool HW504::value(int button)
    {
        switch (button)
        {
        case HW_LEFT_BUTTON:
            return this->_x_axis() < 100;
        case HW_RIGHT_BUTTON:
            return this->_x_axis() > 1000;
        case HW_UP_BUTTON:
            return this->_y_axis() < 100;
        case HW_DOWN_BUTTON:
            return this->_y_axis() > 1000;
        case HW_CENTER_BUTTON:
            return this->_button() == LOW;
        default:
            return false;
        }
    }

    ButtonUART::ButtonUART(float debounce)
    {
        this->serial = new SerialPIO(0, 1);
        this->serial->begin(115200);
        this->debounce = debounce;
        this->startTime = millis();
    }

    void ButtonUART::run()
    {
        if (millis() - this->startTime > this->debounce)
        {
            this->last_button = -1;
            this->startTime = millis();
            // Check if data is available to read
            if (this->serial->available() > 0)
            {
                // Read the incoming byte as a character
                char incomingChar = this->serial->read();
                switch ((int)incomingChar)
                {
                case 48:
                    this->last_button = BUTTON_UP;
                    break;
                case 49:
                    this->last_button = BUTTON_DOWN;
                    break;
                case 50:
                    this->last_button = BUTTON_LEFT;
                    break;
                case 51:
                    this->last_button = BUTTON_RIGHT;
                    break;
                case 52:
                    this->last_button = BUTTON_CENTER;
                    break;
                case 53:
                    this->last_button = BUTTON_BACK;
                    break;
                case 54:
                    this->last_button = BUTTON_START;
                    break;
                default:
                    this->last_button = -1;
                    break;
                }
            }
        }
    }

    Input::Input()
        : pin(-1), button(-1), elapsed_time(0), was_pressed(false), hw(nullptr), bt(nullptr)
    {
    }

    Input::Input(int pin, int button)
    {
        this->pin = pin;
        this->button = button;
        this->elapsed_time = 0;
        this->was_pressed = false;
        this->hw = nullptr;
        this->bt = nullptr;
        pinMode(this->pin, INPUT_PULLUP);
    }

    Input::Input(HW504 *hw, int button)
    {
        this->pin = -1;
        this->button = button;
        this->elapsed_time = 0;
        this->was_pressed = false;
        this->hw = hw;
        this->bt = nullptr;
    }

    Input::Input(ButtonUART *bt)
    {
        this->pin = -1;
        this->button = -1; // not needed
        this->elapsed_time = 0;
        this->was_pressed = false;
        this->hw = nullptr;
        this->bt = bt;
    }

    bool Input::is_pressed()
    {
        if (this->hw)
            return this->hw->value(this->button);
        else if (!this->bt)
            return digitalRead(this->pin) == LOW;
        return false;
    }

    bool Input::is_held(int duration)
    {
        return this->is_pressed() && this->elapsed_time >= duration;
    }

    void Input::run()
    {
        if (this->bt)
            this->bt->run();
        if (this->is_pressed())
            this->elapsed_time++;
        else
            this->elapsed_time = 0;

        if (this->is_pressed() && !this->was_pressed)
            this->was_pressed = true;
        else if (!this->is_pressed())
            this->was_pressed = false;
    }
    Input::operator bool() const
    {
        return this->hw ? this->hw != nullptr : this->bt ? this->bt != nullptr
                                                         : this->pin != -1;
    }
}