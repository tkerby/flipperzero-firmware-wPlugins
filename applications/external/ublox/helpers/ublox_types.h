#pragma once

#define UBLOX_VERSION_APP FAP_VERSION
#define UBLOX_DEVELOPED   "liamhays"
#define UBLOX_GITHUB      "https://github.com/liamhays/ublox"

#define UBLOX_KML_EXTENSION ".kml"
#define UBLOX_GPX_EXTENSION ".gpx"

typedef enum {
    UbloxLogStateStartLogging,
    UbloxLogStateLogging,
    UbloxLogStateStopLogging,
    UbloxLogStateNone,
} UbloxLogState;

typedef enum {
    UbloxDataDisplayViewModeHandheld,
    UbloxDataDisplayViewModeCar,
} UbloxDataDisplayViewMode;

typedef enum {
    UbloxDataDisplayBacklightOn,
    UbloxDataDisplayBacklightDefault,
} UbloxDataDisplayBacklightMode;

typedef uint32_t UbloxDataDisplayRefreshRate;

typedef enum {
    UbloxDataDisplayNotifyOn,
    UbloxDataDisplayNotifyOff,
} UbloxDataDisplayNotifyMode;

typedef enum {
    UbloxOdometerModeRunning = 0,
    UbloxOdometerModeCycling = 1,
    UbloxOdometerModeSwimming = 2,
    UbloxOdometerModeCar = 3,
} UbloxOdometerMode;

typedef enum {
    UbloxPlatformModelPortable = 0,
    UbloxPlatformModelPedestrian = 3,
    UbloxPlatformModelAutomotive = 4,
    UbloxPlatformModelAtSea = 5,
    UbloxPlatformModelAirborne2g = 7,
    UbloxPlatformModelWrist = 9,
} UbloxPlatformModel;

typedef enum {
    UbloxLogFormatKML,
    UbloxLogFormatGPX,
} UbloxLogFormat;

typedef struct UbloxDataDisplayState {
    UbloxDataDisplayViewMode view_mode;
    UbloxDataDisplayBacklightMode backlight_mode;
    UbloxDataDisplayRefreshRate refresh_rate;
    UbloxDataDisplayNotifyMode notify_mode;
    UbloxLogFormat log_format;
} UbloxDataDisplayState;

typedef struct UbloxDeviceState {
    UbloxOdometerMode odometer_mode;
    UbloxPlatformModel platform_model;
} UbloxDeviceState;
