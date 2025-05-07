## Requirements
- WiFi Developer Board, Raspberry Pi, or ESP32 device flashed with [FlipperHTTP v1.8.2](https://github.com/jblanked/FlipperHTTP) or higher.
- 2.4 GHz WiFi access point

## Installation
To install FlipWorld, visit https://lab.flipper.net/apps/flip_world or download the `flip_world.uf2` file from the `FlipperZero/build` directory. Then use qFlipper to add it to the `apps/GPIO` folder on your Flipper Zero.

## How It Works

FlipWorld and FlipSocial are connected. Your login information is the same in both apps; if you register an account in either app, you can log in to both using that information. This also means that your friends, messages, and achievements are synced between apps. You only need a username and password to start, which are set in the User Settings. Keep in mind your username will be displayed to others, so choose wisely.

**Settings**

- **WiFi**: Enter your SSID and password to connect to your 2.4 GHz network.
- **User**: Add or update your username and password (this is the same login information as your FlipSocial account).
- **Game**: Install the Official World Pack, choose your weapon, set your FPS (30, 60, 120, or 240), and select whether you want the screen backlight to always be on, the sound to be on, and the vibration to be on.

**Controls**

- **Press/Hold LEFT**: Turn left if not already facing left, then walk left if the button is still pressed.
- **Press/Hold RIGHT**: Turn right if not already facing right, then walk right if the button is still pressed.
- **Press/Hold UP**: Walk up.
- **Press/Hold DOWN**: Walk down.
- **Press OK**: Interact, attack, or teleport. Attacks enemies when colliding with them until all enemies are defeated. Interacts with NPCs when colliding with them.
- **HOLD OK**: In-Game Menu.
- **Press BACK**: Leave the menu.
- **HOLD BACK**: Exit the game.

## Short Tutorial

1. Ensure your WiFi Developer Board and Video Game Module are flashed with FlipperHTTP.
2. Install the app.
3. Restart your Flipper Zero, then open FlipWorld.
4. Click `Settings -> WiFi`, then input your WiFi SSID and password.
5. Hit the `BACK` button, click `User`. If your username is not present, click `Username` and add one. Do the same for the password field.
6. Go back to the main menu and hit `Play`. It will register an account if necessary and fetch data from our API that's used to render our graphics.
