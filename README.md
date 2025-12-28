# Bzz Bzz - Rhythm Game for Flipper Zero

A rhythm game where players must match vibration patterns by pressing the OK button at the right times.

## Game Concept

Bzz Bzz challenges players to match timing patterns. The game plays a sequence of 5 vibrations with varying pauses between them, then the player must press the OK button at the same rhythm to match the pattern.

## How to Play

1. Press **OK** to start the game
2. Watch the pattern - the Flipper Zero will vibrate 5 short times with varying pauses between (this creates the rhythm pattern you need to remember)
3. Match the rhythm by pressing the **OK** button at the same intervals as the vibration pattern
4. Each successful completion adds a new pattern and increases your counters
5. If you press the button at the wrong time, the same pattern repeats
6. Press **Back** to exit the game

## Game Features

- Fixed 5-element rhythm patterns
- Consistent very short vibration length (25ms each)
- Variable slower pauses between vibrations (200-800ms) create the rhythm pattern
- Counter showing "in_a_row (total)" - consecutive correct patterns and total correct
- No game over - game continues indefinitely
- No sound - only vibration during pattern playback
- No vibration feedback during input - quiet while you input
- Timing tolerance of 50% for more forgiving gameplay
- One-button gameplay using only the OK button
- 1-second delay before showing each new pattern for relaxation
- Simple 3-row UI: Title, status message, and score counter

## Technical Implementation

- Built using the Flipper Zero SDK and uFBT build system
- Uses the notification system for vibration control
- Implements proper GUI rendering and input handling
- Thread-safe implementation with mutex protection
- Timing-based gameplay using Furi tick system

## Building the Game

To build this application, ensure you have uFBT installed:

```bash
python3 -m pip install --upgrade ufbt
```

Then navigate to the project directory and run:

```bash
cd vibration_game
ufbt
```

The resulting `.fap` file will be in the `dist/` directory.

## Controls

- **OK**: Start the game and match the rhythm pattern
- **Back**: Exit the game

## Game States

- **Idle**: Waiting for player to start
- **Showing Pattern**: Playing vibration sequence
- **Waiting for Input**: Player is matching the rhythm