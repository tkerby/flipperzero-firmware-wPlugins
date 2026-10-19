#include "motion_sensor.h"
#include "sensors/ICM42688P.h"

#define TAG "MotionLab"

// 1 kHz matches CAPTURE_RATE_HZ, which the duration and tempo arithmetic
// assumes. Changing one without the other silently skews every timing.
#define SENSOR_RATE DataRate1kHz

// Samples averaged to find the gyro's zero offset at startup.
#define CALIBRATION_SAMPLES 32

typedef enum {
    SensorEventNewData = (1 << 0),
    SensorEventStop = (1 << 1),
} SensorEvent;

#define SENSOR_EVENTS_ALL (SensorEventNewData | SensorEventStop)

struct MotionSensor {
    FuriHalSpiBusHandle* spi;
    ICM42688P* icm;
    FuriThread* thread;
    bool present;
    bool running;

    MotionSensorCallback callback;
    void* context;
};

static void motion_sensor_irq(void* context) {
    furi_assert(context);
    MotionSensor* ms = context;
    furi_thread_flags_set(furi_thread_get_id(ms->thread), SensorEventNewData);
}

// Measure and store the gyro's zero offset. Without this a stationary device
// reports a slow constant rotation, which would drift the direction-reversal
// detection that swing tempo depends on.
static void motion_sensor_calibrate(MotionSensor* ms) {
    ICM42688PScaledData zero = {.x = 0.0f, .y = 0.0f, .z = 0.0f};
    icm42688p_write_gyro_offset(ms->icm, &zero);
    furi_delay_ms(10);

    int32_t sum_x = 0, sum_y = 0, sum_z = 0;
    ICM42688PRawData raw;
    for(uint8_t i = 0; i < CALIBRATION_SAMPLES; i++) {
        icm42688p_read_gyro_raw(ms->icm, &raw);
        sum_x += raw.x;
        sum_y += raw.y;
        sum_z += raw.z;
        furi_delay_ms(2);
    }

    raw.x = sum_x / CALIBRATION_SAMPLES;
    raw.y = sum_y / CALIBRATION_SAMPLES;
    raw.z = sum_z / CALIBRATION_SAMPLES;

    ICM42688PScaledData offset;
    icm42688p_apply_scale(&raw, icm42688p_gyro_get_full_scale(ms->icm), &offset);
    icm42688p_write_gyro_offset(ms->icm, &offset);
}

static int32_t motion_sensor_thread(void* context) {
    furi_assert(context);
    MotionSensor* ms = context;

    motion_sensor_calibrate(ms);

    // 16 g and 2000 deg/s are the widest ranges the part offers, and a bat or
    // club swing genuinely clips anything narrower.
    icm42688p_accel_config(ms->icm, AccelFullScale16G, SENSOR_RATE);
    icm42688p_gyro_config(ms->icm, GyroFullScale2000DPS, SENSOR_RATE);

    icm42688_fifo_enable(ms->icm, motion_sensor_irq, ms);

    while(true) {
        const uint32_t events =
            furi_thread_flags_wait(SENSOR_EVENTS_ALL, FuriFlagWaitAny, FuriWaitForever);

        if(events & SensorEventStop) break;

        if(events & SensorEventNewData) {
            uint16_t pending = icm42688_fifo_get_count(ms->icm);
            while(pending--) {
                ICM42688PFifoPacket packet;
                // Despite returning bool, this is NOT a success flag: it hands
                // back bit 7 of the FIFO header, which the sensor sets to mark
                // an empty-FIFO record rather than a sample. So a true here
                // means "nothing left to read", and a valid sample reads false.
                // Flipper's own game engine sidesteps the trap by ignoring the
                // return value entirely.
                if(icm42688_fifo_read(ms->icm, &packet)) break;

                ICM42688PScaledData accel, gyro;
                icm42688p_apply_scale_fifo(ms->icm, &packet, &accel, &gyro);

                const CaptureSample sample = {
                    .ax = accel.x,
                    .ay = accel.y,
                    .az = accel.z,
                    .gx = gyro.x,
                    .gy = gyro.y,
                    .gz = gyro.z,
                };
                if(ms->callback) ms->callback(&sample, ms->context);
            }
        }
    }

    icm42688_fifo_disable(ms->icm);
    return 0;
}

MotionSensor* motion_sensor_alloc(void) {
    MotionSensor* ms = malloc(sizeof(MotionSensor));
    memset(ms, 0, sizeof(MotionSensor));

    // The module's sensor hangs off the Flipper's external SPI bus, with chip
    // select on PC3 and its interrupt line on PB2.
    ms->spi = malloc(sizeof(FuriHalSpiBusHandle));
    memcpy(ms->spi, &furi_hal_spi_bus_handle_external, sizeof(FuriHalSpiBusHandle));
    ms->spi->cs = &gpio_ext_pc3;

    ms->icm = icm42688p_alloc(ms->spi, &gpio_ext_pb2);
    ms->present = icm42688p_init(ms->icm);
    if(!ms->present) FURI_LOG_W(TAG, "no motion sensor found");

    return ms;
}

void motion_sensor_free(MotionSensor* ms) {
    furi_assert(ms);
    motion_sensor_stop(ms);
    icm42688p_deinit(ms->icm);
    icm42688p_free(ms->icm);
    free(ms->spi);
    free(ms);
}

bool motion_sensor_present(MotionSensor* ms) {
    return ms->present;
}

void motion_sensor_start(MotionSensor* ms, MotionSensorCallback callback, void* context) {
    furi_assert(ms);
    if(!ms->present || ms->running) return;

    ms->callback = callback;
    ms->context = context;
    ms->thread = furi_thread_alloc_ex("MotionSensor", 2048, motion_sensor_thread, ms);
    // Above the UI so a full FIFO is always drained before it overflows.
    furi_thread_set_priority(ms->thread, FuriThreadPriorityHigh);
    furi_thread_start(ms->thread);
    ms->running = true;
}

void motion_sensor_stop(MotionSensor* ms) {
    furi_assert(ms);
    if(!ms->running) return;

    furi_thread_flags_set(furi_thread_get_id(ms->thread), SensorEventStop);
    furi_thread_join(ms->thread);
    furi_thread_free(ms->thread);
    ms->thread = NULL;
    ms->running = false;
    ms->callback = NULL;
}
