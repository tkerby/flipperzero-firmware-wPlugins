# Free Roam
3D Open World Multiplayer Game for the Flipper Zero 

## Requirements
- WiFi Developer Board, BW16, Raspberry Pi, or ESP32 device flashed with FlipperHTTP v2.0 or higher: https://github.com/jblanked/FlipperHTTP
- 2.4 GHz or 5 GHz Wi-Fi access point

## Connect Online
- Discord: https://discord.gg/5aN9qwkEc6
- YouTube: https://www.youtube.com/@jblanked
- Instagram: https://www.instagram.com/jblanked
- Other: https://www.jblanked.com/social/

## Features
- Login with your FlipSocial/FlipWorld account
- Third person camera
- Dynamic map generation (for developers)
- 3D sprites (humans, trees, etc.)
- PVE Multiplayer - COMING SOON


## Current Flipper Zero Bugs
- You can't run the CLI and send a httpRequest at the same time, otherwise it freezes.
- Occasional "MPU fault, possibly stack overflow" error after clicking "Welcome" in the welcome view.
- Occasional "MPU fault, possibly stack overflow" error after clicking "Local" in the lobby menu view.
- Occasional freeze when clicking "Leave Game" in the system info menu settings tab.
- (Developer note) I couldn't load the user info immediately after login, so I had to do it after they hit "Local" or "Online" in the lobby menu. I would get a stack overflow error otherwise as soon as the login was successful.

You will have to restart your device if you encounter any of these bugs.

## How To Play
- Press the left button to turn the camera left
- Press the right button to turn the camera right
- Press the up button to move forward
- Press the down button to move backward
- Press the back button to open the system menu