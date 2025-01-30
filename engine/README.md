## VGM Game Engine
This project is not affiliated with Flipper Devices and utilizes the [PicoDVI](https://github.com/Wren6991/PicoDVI) and [pico-game-engine](https://github.com/jblanked/pico-game-engine) libraries.

### Getting Started
The Flipper Zero app allows you to use your d-pad as input in games created by the engine. The source code is located within the `App` folder. You can use [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) to compile the source code into a `.fap` file.

The Arduino library is used to create and manage games. Copy the `vgm_game_engine` folder from within the `Arduino` folder into your Arduino `libraries` folder (typically located at `User/Documents/Arduino/libraries`). To use the library, add `#include "VGMGameEngine.h"` at the top of your `.ino` file. Review the provided examples to understand how to use the library.