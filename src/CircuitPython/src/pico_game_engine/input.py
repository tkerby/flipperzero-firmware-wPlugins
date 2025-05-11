from digitalio import DigitalInOut, Direction
import microcontroller


BUTTON_UP = 0
BUTTON_DOWN = 1
BUTTON_LEFT = 2
BUTTON_RIGHT = 3
BUTTON_CENTER = 4
BUTTON_BACK = 5
BUTTON_START = 6


class Input:
    """
    Input class to handle the button presses and joystick input.

    @param pin: The pin number of the button.
    @param button: The button to be checked.
    """

    def __init__(self, pin: microcontroller.Pin, button: int):
        self.pin = DigitalInOut(pin)
        self.pin.direction = Direction.OUTPUT
        self.button = button
        self.elapsed_time = 0
        self.was_pressed = False

    def is_pressed(self) -> bool:
        """
        Return True if the button is pressed.
        Since we are pulling up the button, we need to check for a LOW value.
        """
        return self.pin.value

    def is_held(self, duration: int) -> bool:
        """
        Return True if the button is held for a certain duration.
        """

        return self.elapsed_time >= duration

    def run(self):
        """
        Track the button state and the elapsed time it was pressed.
        This should be looped in the main loop.
        """
        if self.is_pressed():
            self.elapsed_time += 1
            self.was_pressed = True
        else:
            self.elapsed_time = 0
            self.was_pressed = False
