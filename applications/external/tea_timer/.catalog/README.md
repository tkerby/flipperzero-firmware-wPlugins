# Tea Timer

Pour-over timer for tea brewing. It counts the time of every pour, tells you when the water
has to be added and keeps track of how many pours are done and how many are left.

## Features

- Presets are plain text files kept in the app data folder on the SD card, so they can be
  made on the device or written by hand in any text editor
- A preset keeps the tea name, the leaf dosage in grams per 100 ml, the recommended water
  temperature range, the time of the first pour, the increment for the next pours and the
  maximum number of pours
- The preset card shows all parameters at once, including the water temperature range, for
  example Water 85-90 C
- Create, edit and delete presets right on the Flipper Zero
- The brew screen stays focused on the timer: a big countdown, the pour counter and the
  current action, nothing else
- Presets written by an older version of the app, without temperature keys, are still
  readable

## How to use

1. Open the app and choose Presets to load an existing preset or New preset to create one
2. On the preset card press Start to begin brewing, or the arrow button to edit or delete
   the preset
3. During a pour the screen shows the countdown; press OK to stop the timer
4. When the pour is over the screen shows Time is up! together with the time of the next
   pour; press OK to start the next pour
5. Press Back to leave the brew screen and return to the preset card

## Water temperature

- The recommended range is 85 to 90 C and is stored in the preset
- A file without both temperature keys is read with the default range 85-90 C
- A file with only one key of the pair is rejected with an error dialog that names the
  missing key
- The value is shown on the preset card only; the brew screen never shows the temperature

## Preset file format

A preset is a text file of at most 1 KB with one key per line:

- name: any name of 1 to 24 characters, latin letters, digits, space and underscore
- leaf_g_per_100ml: leaf dosage from 0.1 to 50.0, for example 5.0
- water_temp_min_c: lower bound of the water temperature, 85 to 90
- water_temp_max_c: upper bound of the water temperature, 85 to 90, not less than
  water_temp_min_c
- first_pour_sec: duration of the first pour in seconds, 1 to 3600
- increment_sec: how much longer every next pour is, in seconds, 0 to 600
- max_pours: recommended number of pours, 1 to 99

Lines that start with a hash sign are comments and are ignored.
