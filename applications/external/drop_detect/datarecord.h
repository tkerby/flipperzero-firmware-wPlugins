#pragma once
#include <furi.h>
ICM42688P* get_imu(void);
void capture(ICM42688P* imu, uint32_t samples, const char* label);
bool capture_is_done(void);
void capture_cancel(void);
void capture_join(void);