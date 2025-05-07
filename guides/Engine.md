# Engine

The engine is available for the Arduino IDE (C++), MicroPython, and C/C++ (using the official SDK). Each environment offers similar configurations, methods, and functionalities to ensure a smooth development experience.

### Arduino IDE

Use the Arduino IDE library to create and manage games. Copy the `pico_game_engine` folder from the `src/ArduinoIDE` directory into your Arduino `libraries` folder (usually `~/Documents/Arduino/libraries`). Then add this at the top of your `.ino` file:

```cpp
#include "PicoGameEngine.h"
```

Review the included examples to learn how to use the library.

### MicroPython

MicroPython support is a work in progress. The PicoDVI library currently does not support MicroPython. The PicoCalc Kit and other TFT boards already support MicroPython; once the PicoDVI library is ported, they will be supported too. To install:

1. Download the appropriate UF2 file for your Pico from the `builds/Engine` directory.  
2. Press and hold the `BOOT` button on your Pico for two seconds.  
3. While still holding `BOOT`, connect your Pico to your computer via USB.  
4. Drag and drop the UF2 file onto the mounted device; it will reboot and start running MicroPython.  
5. Download and install Thonny IDE (https://thonny.org).  
6. In the bottom-right corner of Thonny, select your Pico as the target device.  
7. Create a new script and save it as `main.py` on your Pico, or import an example via the **Files** panel.  
8. Reboot your Pico; it will automatically run `main.py`.

### C/C++ SDK

This integration is not yet available.