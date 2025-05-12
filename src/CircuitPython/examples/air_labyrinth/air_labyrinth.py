"""
Air Labyrinth
Author: JBlanked
Original Author: Derek Jamison
Translated: https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/vgm/apps/air_labyrinth
"""

# import all the modules
from gc import collect, mem_free


def report():
    collect()
    print(mem_free())


from pico_game_engine.game import Game
from pico_game_engine.engine import GameEngine
from pico_game_engine.entity import Entity
from pico_game_engine.level import Level
from pico_game_engine.input import (
    Input,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_RIGHT,
    BUTTON_LEFT,
    BUTTON_UART,
)
from picogui.vector import Vector
from picogui.image import Image
from picogui.draw import Draw, TFT_BLACK, TFT_WHITE
from picogui.boards import VGM_BOARD_CONFIG, JBLANKED_BOARD_CONFIG
from uart import UART


class Wall:
    """
    Wall class
    """

    def __init__(self, is_horizontal: bool, x: int, y: int, length: int):
        self.is_horizontal = is_horizontal
        self.x = x
        self.y = y
        self.length = length


def wall(is_horizontal: bool, y: int, x: int, length: int):
    """
    Wall function
    """

    return Wall(is_horizontal, (x * 320) / 128, (y * 240) / 64, length)


walls: list[Wall] = [
    wall(True, 12, 0, 3),
    wall(False, 3, 3, 17),
    wall(False, 23, 3, 6),
    wall(True, 3, 4, 57),
    wall(True, 28, 4, 56),
    wall(False, 4, 7, 5),
    wall(False, 12, 7, 13),
    wall(True, 8, 8, 34),
    wall(True, 12, 8, 42),
    wall(True, 24, 8, 8),
    wall(True, 16, 11, 8),
    wall(False, 17, 11, 4),
    wall(True, 20, 12, 22),
    wall(False, 6, 17, 2),
    wall(True, 24, 19, 15),
    wall(True, 16, 22, 16),
    wall(False, 4, 24, 1),
    wall(False, 21, 28, 2),
    wall(False, 6, 33, 2),
    wall(False, 13, 34, 3),
    wall(False, 17, 37, 11),
    wall(True, 16, 41, 14),
    wall(False, 20, 41, 5),
    wall(True, 20, 45, 12),
    wall(True, 24, 45, 12),
    wall(False, 4, 46, 2),
    wall(False, 9, 46, 3),
    wall(False, 6, 50, 3),
    wall(True, 12, 53, 7),
    wall(True, 8, 54, 6),
    wall(False, 4, 60, 19),
    wall(False, 26, 60, 6),
]


class AirLabyrinth:
    """
    Pico Game Engine
    """

    def __init__(self):
        # create a new TFT display
        self.screen = Draw(VGM_BOARD_CONFIG)
        # create a new game
        self.game: Game = None

        # set the current level to None
        self.current_level: Level = None

        # set the engine to None
        self.engine = None

    def add_input(self):
        """Add input controls"""
        self.game.input_add(Input(button=BUTTON_UART, uart=UART()))

    def add_level(self):
        """Add a new level"""
        self.current_level = Level(
            name="Level 1", size=Vector(320, 240), game=self.game
        )
        self.game.level_add(self.current_level)

    def constrain(self, value, min_value, max_value):
        """Clamp a value between min_value and max_value."""
        return max(min(value, max_value), min_value)

    def player_render(self, player: Entity, draw: Draw, game: Game):
        """Render the player entity"""
        draw.text(Vector(100, 6), "@codeallnight - ver 0.1", TFT_BLACK)
        draw.text(Vector(100, 224), "pico game engine demo", TFT_BLACK)

    def player_spawn(self):
        """Add a new player entity"""

        player_entity = Entity(
            name="Player",
            position=Vector(160, 130),
            sprite_file_path="/sd/player.bmp",
            sprite_size=Vector(10, 10),
            update=self.player_update,
            render=self.player_render,
            is_player=True,
        )

        self.current_level.entity_add(player_entity)

    def player_update(self, player: Entity, game: Game):
        """Update the player entity"""
        old_position = player.position
        new_position = old_position

        # Move according to input
        if game.input == BUTTON_UP:
            new_position.y -= 5

        elif game.input == BUTTON_DOWN:
            new_position.y += 5

        elif game.input == BUTTON_LEFT:
            new_position.x -= 5

        elif game.input == BUTTON_RIGHT:
            new_position.x += 5

        # reset input
        game.input = -1

        # Tentatively set new position
        player.position = new_position

        # check if new position is within the level boundaries
        if (
            new_position.x < 0
            or new_position.x + player.size.x > self.current_level.size.x
            or new_position.y < 0
            or new_position.y + player.size.y > self.current_level.size.y
        ):
            # restore old position
            player.position = old_position

        # Update camera position to center the player
        camera_x = player.position.x - (game.size.x / 2)
        camera_y = player.position.y - (game.size.y / 2)

        # Clamp camera position to the world boundaries
        camera_x = self.constrain(camera_x, 0, self.current_level.size.x - game.size.x)
        camera_y = self.constrain(camera_y, 0, self.current_level.size.y - game.size.y)

        # Set the new camera position
        game.position = Vector(camera_x, camera_y)

    def wall_collision(self, player: Entity, other: Entity, game: Game):
        '''""Handle wall collision"""'''
        if other.name == "Player":
            other.position = other.position_old

    def wall_render(self, player: Entity, draw: Draw, game: Game):
        """Render the wall entity"""
        draw.rect_fill(player.position, player.size, TFT_BLACK)

    def wall_start(self, position: Vector, size: Vector):
        """Helper function to create a wall entity"""
        real_position = Vector(position.x - size.x / 2, position.y - size.y / 2)
        wall_entity = Entity(
            name="Wall",
            position=real_position,
            sprite_file_path="",
            sprite_size=size,
            render=self.wall_render,
            collision=self.wall_collision,
            is_player=False,
        )
        self.current_level.entity_add(wall_entity)

    def wall_spawn(self):
        scale = 2

        for temp in walls:
            # 1) unscaled size in pixels (integer division)
            if temp.is_horizontal:
                w = temp.length * 320 // 128  # e.g. (3*320)//128 == 7
                h = 1 * 240 // 64  # == 3
            else:
                w = 1 * 320 // 128  # == 2
                h = temp.length * 240 // 64

            # 2) apply scale
            size_x = w * scale  # 7*2 == 14
            size_y = h * scale  # 3*2 == 6
            size = Vector(size_x, size_y)

            # 3) compute center with integer arithmetic
            center_x = temp.x * scale + size_x // 2
            center_y = temp.y * scale + size_y // 2

            # 4) wall_start subtracts half‑size to get top‑left
            self.wall_start(Vector(center_x, center_y), size)

    def run(self):
        """Run the game engine"""

        # create a new game
        self.game = Game(
            name="Air Labyrinth",
            start=None,  # no custom start
            stop=None,  # no custom stop
            draw=self.screen,
            foreground_color=TFT_BLACK,
            background_color=TFT_WHITE,
        )

        self.game.position = Vector(160, 130)

        # add controls, level, and player
        self.add_input()
        self.add_level()
        self.wall_spawn()
        self.player_spawn()

        # create a new game engine
        self.engine = GameEngine(
            fps=30,
            game=self.game,
        )

        # run the game engine
        try:
            self.engine.run()
        except KeyboardInterrupt:
            self.game.is_running = False
        finally:
            self.game.stop()


# run the main function
if __name__ == "__main__":
    report()
    pico_game_engine = AirLabyrinth()
    report()
    pico_game_engine.run()
