from gc import collect as free
from picogui.vector import Vector
from picogui.draw import Draw
from .level import Level
from .input import (
    Input,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_CENTER,
    BUTTON_BACK,
    BUTTON_START,
)


class Game:
    """
    Represents a game.

    Parameters:
    - name: str - the name of the game
    - draw: Draw - the draw object to be used for rendering
    - foreground_color: int - the color of the foreground
    - background_color: int - the color of the background
    - start: function() - the function called when the game is created
    - stop: function() - the function called when the game is destroyed
    """

    def __init__(
        self,
        name: str,
        draw: Draw,
        foreground_color: int,
        background_color: int,
        start=None,
        stop=None,
    ):
        self.name = name
        self._start = start
        self._stop = stop
        self.levels: list[Level] = []  # List of levels in the game
        self.current_level: Level = None  # holds the current level
        self.button_up: Input = None
        self.button_down: Input = None
        self.button_left: Input = None
        self.button_right: Input = None
        self.button_center: Input = None
        self.button_back: Input = None
        self.button_start: Input = None
        self.button_uart: Input = None
        self.input: int = -1  # last button pressed
        self.draw = draw
        self.camera = Vector(0, 0)
        self.position = Vector(0, 0)
        self.size = Vector(draw.size.x, draw.size.y)
        self.world_size = Vector(draw.size.x, draw.size.y)
        self.is_active = False
        self.foreground_color = foreground_color
        self.background_color = background_color
        self.is_uart_input = False

    def clamp(self, value, lower, upper):
        """Clamp a value between a lower and upper bound."""
        return min(max(value, lower), upper)

    @property
    def is_running(self) -> bool:
        """Return the running state of the game"""
        return self.is_active

    @is_running.setter
    def is_running(self, value: bool):
        """Set the running state of the game"""
        self.is_active = value

    def input_add(self, control: Input):
        """Add an input control to the game"""
        if control.uart:
            self.button_uart = control
            self.is_uart_input = True
        elif control.button == BUTTON_UP:
            self.button_up = control
        elif control.button == BUTTON_DOWN:
            self.button_down = control
        elif control.button == BUTTON_LEFT:
            self.button_left = control
        elif control.button == BUTTON_RIGHT:
            self.button_right = control
        elif control.button == BUTTON_CENTER:
            self.button_center = control
        elif control.button == BUTTON_BACK:
            self.button_back = control
        elif control.button == BUTTON_START:
            self.button_start = control

    def input_remove(self, control: Input):
        """Remove an input control"""
        if control.uart:
            self.button_uart = None
            self.is_uart_input = False
        elif control.button == BUTTON_UP:
            self.button_up = None
        elif control.button == BUTTON_DOWN:
            self.button_down = None
        elif control.button == BUTTON_LEFT:
            self.button_left = None
        elif control.button == BUTTON_RIGHT:
            self.button_right = None
        elif control.button == BUTTON_CENTER:
            self.button_center = None
        elif control.button == BUTTON_BACK:
            self.button_back = None
        elif control.button == BUTTON_START:
            self.button_start = None
        free()

    def level_add(self, level: Level):
        """Add a level to the game"""
        self.levels.append(level)

    def level_remove(self, level: Level):
        """Remove a level from the game"""
        self.levels.remove(level)

    def level_switch(self, level: Level):
        """Switch to a new level"""
        if not level:
            print("Level is not valid.")
            return
        old_level = self.current_level
        self.current_level = level
        old_level.stop()
        old_level.clear()
        self.current_level.start()

    def manage_input(self):
        """Check for input from the user"""
        if self.is_uart_input and self.button_uart:
            self.input = self.button_uart.last_button
        elif self.button_up and self.button_up.is_pressed():
            self.input = BUTTON_UP
        elif self.button_down and self.button_down.is_pressed():
            self.input = BUTTON_DOWN
        elif self.button_left and self.button_left.is_pressed():
            self.input = BUTTON_LEFT
        elif self.button_right and self.button_right.is_pressed():
            self.input = BUTTON_RIGHT
        elif self.button_center and self.button_center.is_pressed():
            self.input = BUTTON_CENTER
        elif self.button_back and self.button_back.is_pressed():
            self.input = BUTTON_BACK
        elif self.button_start and self.button_start.is_pressed():
            self.input = BUTTON_START
        else:
            self.input = -1

    def render(self):
        """Render the current level"""
        if self.current_level:
            self.current_level.render()

    def start(self) -> bool:
        """Start the game"""
        if not self.levels:
            print("The game has no levels.")
            return False
        self.current_level = self.levels[0]
        if self._start:
            self._start(self)
        self.draw.fill(self.background_color)
        self.current_level.start()
        self.is_active = True
        free()
        return True

    def stop(self):
        """Stop the game"""

        if not self.is_active:
            return

        if self._stop:
            self._stop(self)

        self.is_active = False

        for level in self.levels:
            if level:
                level.clear()
                level = None
        self.levels = []

        # Clear and release input controls.
        self.button_up = None
        self.button_down = None
        self.button_left = None
        self.button_right = None
        self.button_center = None
        self.button_back = None
        self.button_start = None

        self.draw.fill(self.background_color)
        free()

    def update(self):
        """Update the game input and entity positions in a thread-safe manner."""
        if self.is_uart_input:
            self.button_uart.run()
        else:
            if self.button_up:
                self.button_up.run()
            if self.button_down:
                self.button_down.run()
            if self.button_left:
                self.button_left.run()
            if self.button_right:
                self.button_right.run()
            if self.button_center:
                self.button_center.run()
            if self.button_back:
                self.button_back.run()

        self.manage_input()

        # Run user-defined update functions for each entity.
        for entity in self.current_level.entities:
            if entity.update:
                entity.update(self)

        # Calculate camera offset to center the player.
        self.camera.x = self.position.x - (self.size.x // 2)
        self.camera.y = self.position.y - (self.size.y // 2)

        # Clamp camera position to prevent going outside the world.
        self.camera.x = self.clamp(self.camera.x, 0, self.world_size.x - self.size.x)
        self.camera.y = self.clamp(self.camera.y, 0, self.world_size.y - self.size.y)

        # update the level
        if self.current_level:
            self.current_level.update()
