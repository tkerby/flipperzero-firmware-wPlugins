# AI Composer

A tiny neural network that writes brand-new melodies on your Flipper Zero and plays them on its speaker. Everything runs on the device: no internet, no computer, no samples.

## Features

- Eight styles: Chiptune, Blues, Classical, Folk, Game, Phonk, Hip-Hop and Jazz
- Blend two styles, for example 70 % Classical and 30 % Jazz
- Creativity in five steps, from careful to wild
- Any key, fixed or random, and adjustable volume
- Watch the network write the notes on a piano roll, then hear it play with the LED blinking on every bar
- Save melodies as .fmf files for the built-in Music Player

## Controls

- **Studio**: Up and Down select a setting, Left and Right change it, OK composes
- **Player**: OK composes a new melody, Left saves it, Right replays it, Back returns to the studio

Saved melodies go to apps_data/ai_composer on the SD card. Open them with the Music Player.

## How it works

The network has 32,834 parameters (36.8 KB). It learned from 64,000 melodies written by a rule-based music teacher, so no third-party songs are involved. The speaker plays one note at a time, so every style is a melody line without beat or bass.

Source code and training scripts: https://github.com/TheProductAgency/flipper-ai-composer
