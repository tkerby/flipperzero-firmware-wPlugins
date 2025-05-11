from digitalio import DigitalInOut, Direction
from microcontroller import Pin
from uart import UART


BUTTON_UART = -1
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

    def __init__(self, button: int, pin: Pin = None, uart: UART = None):
        if not pin and not uart:
            raise ValueError("Either pin or uart must be provided.")
        elif pin and uart is None:
            self.pin = DigitalInOut(pin)
            self.pin.direction = Direction.OUTPUT
            self.uart = None
        else:
            self.pin = None
            self.uart = uart

        self.button = button
        self.elapsed_time = 0
        self.was_pressed = False
        self.last_button = 0

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
        if self.uart:
            # Check if data is available to read
            if self.uart.available() > 0:
                # Read the incoming byte as a character
                incoming_char = ord(self.uart.read())
                if incoming_char == 48:
                    self.last_button = BUTTON_UP
                elif incoming_char == 49:
                    self.last_button = BUTTON_DOWN
                elif incoming_char == 50:
                    self.last_button = BUTTON_LEFT
                elif incoming_char == 51:
                    self.last_button = BUTTON_RIGHT
                elif incoming_char == 52:
                    self.last_button = BUTTON_CENTER
                elif incoming_char == 53:
                    self.last_button = BUTTON_BACK
                elif incoming_char == 54:
                    self.last_button = BUTTON_START
                else:
                    self.last_button = -1
            else:
                self.last_button = -1
            return
        if self.is_pressed():
            self.elapsed_time += 1
            self.was_pressed = True
        else:
            self.elapsed_time = 0
            self.was_pressed = False
