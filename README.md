# mitzi-puck
<img alt="Main Puck Girl Screen"  src="screenshots/MainScreen.png" width="40%" />
A simple Flipper Zero arcade chase game. The player controls a pie-shaped character ("Puck girl") through a maze, collecting dots while avoiding ghosts. Ppower pills temporarily make ghosts vulnerable.
Like in the classic game, each ghost has distinct personality: one targets directly, another one patrols. 

## Usage
- **Arrow Keys**: Move Puck-girl through the maze
- **OK Button**: Restart game (when game over or won)
- **Back Button**: Pauses game or (when held) exits

## Scores
- 10 points for normal dot
- 50 for power pills
- 200 for eating ghosts

## Remarks on the implementation
Drawing of the screen occurs back to front:
1. **Maze Background:** Pre-rendered static (full-screen) image
2. **Pellets:** Dots and power pills
3. **Puck-girl:** Animated sprite with directional variants
4. **Ghosts:** 2-frame wobbeling
5. **HUD (= head up display):** Score, lives, power timer, version
6. **Navigation Hints** 
7. **Modals:** Pause/died/game over/win messages

## Version history
See [changelog.md](changelog.md)
