#include "input.h"

namespace VGMGameEngine
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

    ButtonUART::ButtonUART()
    {
        this->serial = new SerialPIO(0, 1);
        this->serial->begin(115200);
    }

    bool ButtonUART::value(int button)
    {
        // read one byte and compare it to the button value
        char incomingChar = this->serial->read();
        if (button == BUTTON_UP && strcmp(&incomingChar, "0") == 0)
            return true;
        else if (button == BUTTON_DOWN && strcmp(&incomingChar, "1") == 0)
            return true;
        else if (button == BUTTON_LEFT && strcmp(&incomingChar, "2") == 0)
            return true;
        else if (button == BUTTON_RIGHT && strcmp(&incomingChar, "3") == 0)
            return true;
        else if (button == BUTTON_CENTER && strcmp(&incomingChar, "4") == 0)
            return true;
        else if (button == BUTTON_BACK && strcmp(&incomingChar, "5") == 0)
            return true;
        else if (button == BUTTON_START && strcmp(&incomingChar, "6") == 0)
            return true;

        return false;
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

    Input::Input(ButtonUART *bt, int button)
    {
        this->pin = -1;
        this->button = button;
        this->elapsed_time = 0;
        this->was_pressed = false;
        this->hw = nullptr;
        this->bt = bt;
    }

    bool Input::is_pressed()
    {
        return this->hw ? this->hw->value(this->button) : this->bt ? this->bt->value(this->button)
                                                                   : digitalRead(this->pin) == LOW;
    }

    bool Input::is_held(int duration)
    {
        return this->is_pressed() && this->elapsed_time >= duration;
    }

    void Input::run()
    {
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