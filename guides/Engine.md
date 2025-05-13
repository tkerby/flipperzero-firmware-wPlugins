# Engine

The engine is available for the Arduino IDE (C++), CircuitPython, and C/C++ (using the official SDK). Each environment offers similar configurations, methods, and functionalities to ensure a smooth development experience.

### Arduino IDE

Use the Arduino IDE library to create and manage games. Copy the `pico_game_engine` folder from the `src/ArduinoIDE` directory into your Arduino `libraries` folder (usually `~/Documents/Arduino/libraries`). Then, add this at the top of your `.ino` file:

```cpp
#include "PicoGameEngine.h"
```

Review the included examples to learn how to use the library.

### CircuitPython

Developers can also use CircuitPython to create and manage games, although due to limited memory, they must be more conservative with memory usage. Perform all heap allocations before the engine starts.

1. Download and install Thonny IDE (https://thonny.org).
2. Download this repository as a .zip file (available here: https://github.com/jblanked/pico-game-engine/archive/refs/heads/main.zip).
3. Press and hold the `BOOT` button on your device for 2 seconds.
4. While holding the `BOOT` button, connect the device to your computer using a USB cable.
5. Open Thonny, and in the bottom-right corner, select `Install CircuitPython`.
6. Change the settings to your specific Raspberry Pi Pico type and proceed with the installation.
7. Once finished, close the window and press the red `Stop/Restart` button.
8. Afterwards, open the .zip file downloaded earlier and navigate to the `src/CircuitPython/src` folder. Copy and paste all contents of that folder into the `lib` folder in your `CIRCUITPY` drive that appeared after CircuitPython finished installing.
9. Inside the .zip file (in the `src/CircuitPython/examples` folder), there are examples that you can copy and paste into your `CIRCUITPY` drive. These should be in your root directory (or optionally the `/sd` directory). The `.bmp` assets should be placed in the `/sd` directory.
10. In Thonny, press the `Stop/Restart` button again, then double-click the example you added to your `CIRCUITPY` drive and click the green `Start/Run` button.

### C/C++ SDK

This integration is not yet available.