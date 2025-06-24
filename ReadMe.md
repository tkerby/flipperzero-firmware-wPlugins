# About

CO2 Logger app for Flipper Zero using MH-Z19 sensor. Uses UART protocol to receive data and logs CO2 readings to CSV file with configurable intervals (15s, 30s, or 60s) and timestamps.

## Credits

This project is based on the original [MH-Z19 UART app](https://github.com/skotopes/flipperzero_co2_logger_uart) by [Aku](https://github.com/skotopes). Enhanced with CSV logging functionality for data collection and analysis.

# Pinout

Flipper | Sensor
--------|-------
1       | 5V
8       | GND
15      | RX
16      | TX


# User Interface

## Main Screen
- **Large CO2 reading** display in PPM
- **Connection timer** showing how long the sensor has been connected
- **RIGHT button**: Enter settings

## Settings Screen  
- **Logging interval configuration**: Choose between 15s, 30s, or 60s
- **UP/DOWN buttons**: Change interval
- **LEFT button**: Return to main screen

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
- Sensor warmup period: No logging for the first 60 seconds to avoid initialization values
- Only logs when sensor is connected and providing valid data
- Configurable logging intervals: 15s, 30s, or 60s (default: 30s)
