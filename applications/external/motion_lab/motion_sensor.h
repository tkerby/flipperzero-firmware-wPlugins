// Motion Lab — the Video Game Module's motion sensor.
//
// Wraps the ICM-42688-P on the Flipper's external SPI bus. The module's sensor
// is wired to both the RP2040 and the Flipper's GPIO connector, so this reads
// it directly and the module can keep running its stock firmware.

#pragma once

#include "capture.h"

#include <furi.h>

// Called once per sample from the sensor's own thread, at CAPTURE_RATE_HZ.
// Keep the work small and do not block.
typedef void (*MotionSensorCallback)(const CaptureSample* sample, void* context);

typedef struct MotionSensor MotionSensor;

MotionSensor* motion_sensor_alloc(void);
void motion_sensor_free(MotionSensor* ms);

// False when no module is attached, or its sensor did not answer. The app
// shows an "attach the module" screen in that case rather than failing.
bool motion_sensor_present(MotionSensor* ms);

// Begin streaming. Hold the device still for the first moment after this: the
// gyro's zero offset is measured here, and motion during it biases every
// rotation reading afterwards.
void motion_sensor_start(MotionSensor* ms, MotionSensorCallback callback, void* context);

void motion_sensor_stop(MotionSensor* ms);
