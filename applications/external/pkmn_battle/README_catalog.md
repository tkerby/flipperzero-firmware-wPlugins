# GBC Pokemon Battle

Battle your Pokemon Crystal team against a Flipper Zero. The Flipper acts as the second Game Boy
in a Cable Club Colosseum link battle: connect a Game Boy Color over a link cable, pick an
opponent, and fight in the real game while the Flipper's battle AI chooses moves.

## Features

- Link battles against an unmodified Pokemon Crystal cartridge (US/Europe)
- Random teams in a chosen level range, or Crystal boss teams (Gym Leaders, Elite Four, Lance,
  Rival, Red, Prof. Oak) with held items and optional level scaling
- Battle AI (damage formula, KOs, status, boosts, recovery, switches)
- Battle music and trainer picture follow the opponent, or can be set manually
- View Team with sprites, levels, items, and moves
- Pinout picker for common link-cable adapters
- Battle logs on the SD card for PC replay

## Hardware

- Flipper Zero (official Release firmware recommended; API must match the FAP)
- Game Boy Color with Pokemon Crystal; save must reach a Pokemon Center 2F Cable Club
- Game Boy link cable to Flipper GPIO (for example KBEmbedded Game Link GPIO Module, Original
  pinout)

Full instructions: https://github.com/cycalo/Flipper-Zero-Game-Boy-Pokemon-Battle
