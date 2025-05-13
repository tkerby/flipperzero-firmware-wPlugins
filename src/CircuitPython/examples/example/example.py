"""
Pico Game Engine
Author: JBlanked
Github: https://github.com/jblanked/pico-game-engine
Info: Create games and animations for Raspberry Pi Pico devices
Created: 2025-05-11
Updated: 2025-05-11
"""

# This is just an example of how to use the Pico Game Engine

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


class PicoGameEngine:
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
        self.game.input_add(Input(button=BUTTON_UART, uart=UART(), debounce=0.05))

    def add_level(self):
        """Add a new level"""
        self.current_level = Level(
            name="Level 1", size=Vector(320, 240), game=self.game
        )
        self.game.level_add(self.current_level)

    def add_player(self):
        """Add a new player entity"""

        player_entity = Entity(
            name="Player",
            position=Vector(160, 120),
            sprite_file_path="/sd/Dino.bmp",
            sprite_size=Vector(16, 16),
            update=self.player_update,
            render=self.player_render,
            is_player=True,
        )

        self.current_level.entity_add(player_entity)

    def game_input(self, player: Entity, game: Game):
        """Move the player entity based on input"""
        if game.input == BUTTON_UP:
            player.position += Vector(0, -5)
        elif game.input == BUTTON_RIGHT:
            player.position += Vector(5, 0)
        elif game.input == BUTTON_DOWN:
            player.position += Vector(0, 5)
        elif game.input == BUTTON_LEFT:
            player.position += Vector(-5, 0)

    def game_start(self, game: Game):
        """Handle your game start logic here"""

    def game_stop(self, game: Game):
        """Handle your game stop logic here"""

    def player_update(self, player: Entity, game: Game):
        """Update the player entity"""

        # move the player entity based on input
        self.game_input(player, game)

    def player_render(self, player: Entity, draw: Draw, game: Game):
        """Render the player entity"""

        # draw name of the game
        draw.text(Vector(110, 10), game.name, TFT_BLACK)

    def run(self):
        """Run the game engine"""

        # create a new game
        self.game = Game(
            name="Example",
            start=self.game_start,
            stop=self.game_stop,
            draw=self.screen,
            foreground_color=TFT_BLACK,
            background_color=TFT_WHITE,
        )

        # add controls, level, and player
        self.add_input()
        self.add_level()
        self.add_player()

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
    pico_game_engine = PicoGameEngine()
    report()
    pico_game_engine.run()
