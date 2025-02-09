# TINA Gauge for Flipper Zero

TINA Gauge is an application for Flipper Zero that allows you to read I2C-connected current/power monitors from Texas Instruments.

## Supported Current/Power Monitors:

- INA219
- INA226
- INA228

## Instructions for Use:

- Connect the sensor to C0 (SCL) and C1 (SDA).
- Launch the application and access the CONFIG menu by pressing the LEFT button to navigate back.
- Select the sensor type, configure its I2C address (default: 0x40), and set the shunt resistance.
- Once the settings are complete, press the BACK button to return to the Gauge Screen.
- If the sensor is connected properly, the Gauge Screen will display the bus voltage and the current flowing through the shunt resistor.








