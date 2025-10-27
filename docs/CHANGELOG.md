v1.1
released app
Feature & Flow Updates
Added SettingsItem_Start to the settings enumeration (enum).

Added a started field to the VEML7700App structure and configured the application to begin on the settings screen with the cursor positioned on "Start".

Modified draw_settings_screen: added the "Start" option, limited the view to 2 items, and included a side scrollbar.

Modified draw_main_screen: the main value is now displayed in a large font, the unit "lx" is shown below the value, and the key legend was removed.

Modified input handling: pressing OK on "Start" sets started = true and returns to the main screen.

The main loop only performs sensor initialization (init) and reading (read) when started == true.

Added support for the WHITE channel (WHITE_MEAS_RESULT_REG) and channel selection in the settings.
