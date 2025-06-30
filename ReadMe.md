# About

App for reading CO2 ppm from MH-Z19 sensor and saving to a local csv file. 

## Credits

This project is based on the original [MH-Z19 UART app](https://github.com/skotopes/flipperzero_co2_logger_uart) by [Aku](https://github.com/skotopes). Enhanced with CSV logging functionality for data collection and analysis.

# Pinout

Flipper | Sensor
--------|-------
1       | 5V
8       | GND
15      | RX
16      | TX


# CSV Logging

The app automatically creates a CSV file at `/ext/apps_data/co2_logger/co2_log.csv` on the SD card and logs valid CO2 readings at the configured interval with the following format:

```
Timestamp,CO2_PPM
2024-01-15 14:30:25,420
2024-01-15 14:30:55,425
```

The CSV file includes:
- Timestamp in YYYY-MM-DD HH:MM:SS format
- CO2 concentration in PPM

**Data Quality Features:**
- Configurable logging intervals: 15s, 30s, or 60s (default: 30s)

# TO DO

There are a few features that I am hoping to add, if you can make any contributions I would greatly appreciate it.
- Toggle the 'AUTO DIM' functionality of the screen. I have added the option to settings but it doesn't work.
- Make a website that can detect if the flipper is connected via USB, read the csv file, and display a line chart. I have built a prototype with Loveable that can detect and connect to the flipper's cli, but I'm having issues reading the csv from the flipper zero. The link to the website is (here)[https://preview--zero-data-explorer.lovable.app/], and the link to the repo is (here)[https://github.com/harryob2/zero-data-explorer]. 
