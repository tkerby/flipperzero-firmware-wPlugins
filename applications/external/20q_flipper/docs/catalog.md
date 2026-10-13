# 20 Questions

Think of anything real: an animal, a food, an object, a place. Your Flipper asks up to 20 yes/no questions and tries to name it. No network, no phone, no cloud. Everything runs on the device.

## How to play

- Pick a rough category, or skip it.
- Answer each question with **Yes**, **probably**, **don't know**, **probably not** or **No**. You don't have to be sure.
- The Flipper gets three guesses. If it loses, teach it your word and it will know it next time.

## Controls

- **Left / Right:** choose an answer, **OK** to confirm
- **Up / Down:** shortcut for Yes / No
- **Back:** undo your last answer
- **Hold Back:** give up

## How it works

There is no neural network on the Flipper. It knows 835 things and 192 questions, stored as a table of probabilities on the SD card. After each answer it updates how likely each thing is, then picks the question that best splits the remaining candidates. In simulated games it wins about 96% of the time when answered honestly.

Words you teach it are saved on the SD card and used in later games.

Source code, data pipeline and details: [github.com/Cheesedog223/20Q_Flipper0](https://github.com/Cheesedog223/20Q_Flipper0)
