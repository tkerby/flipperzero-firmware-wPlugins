## Game rules for standard mancala ("Kalah")
1. Pick up all stones from selected pit.
2. Move counterclockwise placing 1 stone per pit.
3. Skip opponent’s store; place in own store.
4. If last stone lands in own store → take another turn.
5. If last stone lands in an empty pit on your side → capture that stone + opposite pit’s stones into your store.
6. Game ends when one side is empty → other side’s remaining stones flow to their store.
7. Store counts determine winner.

## User flow
* **Left / Right-Button.** Move cursor between the 6 user pits (wrapping is allowed)
* **OK** picks up stones from selected pit and perform the distributions of the stones according to the rules
* Holding **Back** quits the App.

## Simple AI heuristic
1. If choosing a certain pit results in an extra turn, pick it.
2. Else if any pit would result in a capture, pick it.
3. Else choose the leftmost non-empty pit.
