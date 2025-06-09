# About

CO2 Logger app for Flipper Zero using MH-Z19 sensor. Uses UART protocol to receive data and logs CO2 readings to CSV file every 30 seconds with timestamps.

## Credits

This project is based on the original [MH-Z19 UART app](https://github.com/skotopes/flipperzero_co2_logger_uart) by [Aku](https://github.com/skotopes). Enhanced with CSV logging functionality for data collection and analysis.

# Pinout

Flipper | Sensor
--------|-------
1       | 5V
15      | RX
16      | TX
18      | GND

# CSV Logging

The app automatically creates a CSV file at `/ext/apps_data/co2_logger/co2_log.csv` on the SD card and logs CO2 readings every 30 seconds with the following format:

```
Timestamp,CO2_PPM,Status
2024-01-15 14:30:25,420,Connected
2024-01-15 14:30:55,425,Connected
```

The CSV file includes:
- Timestamp in YYYY-MM-DD HH:MM:SS format
- CO2 concentration in PPM
- Connection status (Connected/Disconnected)
