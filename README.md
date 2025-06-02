### Pokemon Showdown for the Flipper Zero

## Features
- Complete Gen 1 Roster
- Turn based Combat
- Type System
- Stats reflected by HP, Attack, Defense, and Speed
- Selection Screen
- Multiple Moves
- Made with Flipper Zero screen 128x64 Monochrome in mind

## How to Play
- Main Menu
-- select '1v1 Battle'
- Scroll through the original 151 to find who you want to battle with
-- Do not select Type Null(I recognize that is a pokemon later but needed to add for accurate Pokedex #s, which i believe was a problem referenced as Misingno in the pkmn_trader repo)
- Choose your opponent
- Use Directional pad to navigate moves
- Center Button(OK) to select
- Back button for back
- Battle to your hearts content

## Installation
# Prerequisites
- Flipper Zero
- qflipper or SD card reader
- Flipper Zero Firmware updated
-- This was built using Rogue Master Custom Firmware(will link soon) Thank you for everything!

# Install Prebuilt .fap
- download 'showdown.fap' from Releases page
- copy to your Flipper's SD card in apps/Games
- Create your dream matchup

# Build from Source
- Clone this repo
	git clone https://github.com/yourusername/flipper-pokemon-showdown.git
	cd flipper-pokemon-showdown
- ./fbt fap_showdown
- your compiled .fap should be in 'firmware root'/build/f7-firmware-C/.extapps/
* I need to enable view hidden files to find .extapps folder 

## Project Structure
- showdown/
	application.fam
	showdown.c 
	showdown.h
	menu.c
	menu.h
	select_screen.c
	select_screen.h
	battle.c
	battle.h
	pokemon.c
	pokemon.h
# Each files purpose
- define the structure for future app creators

## Roadmap
- As you can tell by a quick playthrough, this is a barebones attempt at creating a Pokemon like application. I am currently in school full time and work 40 hours, so I wanted to present this as a sort of demo to show others what i have learned and explore the capabilities of the Flipper Zero.
# Features to explore
- Add sprites to correspond with selections
- Add status effects
- Complex movesets
- Critical/Effective moves
- Notifications when hit
- Animations
- overall improvement and game immersion

# Known Issues
- Currently researching how to get pokemon name to display when in battle
-- originally hardcoded Squirtle and Charmander to for testing and since adding all 151 have not been able to properly change the mechanic that displays the User Pokemon selection's attack/Enemy Pokemon selection's attack

## Acknowledgements
- As much as I hate the current business model, I say naively, Nintendo/Game Freak created an intergenerational product which has given a lot of us very fond memories
- Rogue Master/Malik - I really dont know that I would have had the confidence to push through the C Lang learning curve if it wasnt for being welcomed into the community
- Flipper Zero Team - What an amazing product you all have developed
- Esteban Fuentealba/Kris Bahnsen - I stand on the shoulders of GIANTS! Originally i had planned to use their repos as a base, but had to move to a ground up spot, but their applications and ideas helped make this possible.
- Talking Sasquatch - for instilling an anti skid consciousness haha. Really though, your videos helped me understand the powerful tool at my disposal and show that the flipper is only as good as the person using it.
- Derek Jamison(I hope i spelled that right, cause my youtube search history shows otherwise) Your videos... MWAH *Chefs Kiss*
- My student's
- The flipper community as a whole for all of the documentation and help along the way

### Disclaimer
- This is a fan-made project for educational purposes. Pokemon is a trademark of Nintendo/Game Freak. This project is not affiliated with or endorsed by Nintendo.
- Anybody mentioned is not affiliated with me nor I them, I just wanted to recognize the people and resources that have made this possible.
