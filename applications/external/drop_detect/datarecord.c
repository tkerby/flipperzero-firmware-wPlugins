#include <furi.h>
#include <string.h>
#include <storage/storage.h>
#include "driver/ICM42688P.h"

#define TAG "datarecord"

typedef struct {
    ICM42688P* icm;
    uint32_t samples;
    const char* label;
    bool done;
    bool cancel;
    FuriThread* thread;
} CaptureContext;

// Global capture context for tracking state
static CaptureContext cap_ctx = {0};

int32_t capture_thread(void* context) {
    FURI_LOG_I(TAG, "capture thread start");

    CaptureContext* ctx = context;
    if(!ctx || !ctx->icm) {
        FURI_LOG_E(TAG, "invalid context or IMU");
        return -1;
    }

    Storage* sd = furi_record_open(RECORD_STORAGE);
    if(!sd) {
        FURI_LOG_E(TAG, "open storage failed");
        return -1;
    }
    File* entry = storage_file_alloc(sd);
    if(!entry) {
        FURI_LOG_E(TAG, "alloc file failed");
        furi_record_close(RECORD_STORAGE);
        return -1;
    }

    char filepath[128];
    snprintf(
        filepath,
        sizeof(filepath),
        "/ext/apps_data/drop_detect/%s/%lu.csv",
        ctx->label,
        (unsigned long)furi_get_tick());
    FURI_LOG_I(TAG, "saving to: %s", filepath);

    storage_common_mkdir(sd, "/ext/apps_data/drop_detect");

    char labeldir[64];
    snprintf(labeldir, sizeof(labeldir), "/ext/apps_data/drop_detect/%s", ctx->label);
    storage_common_mkdir(sd, labeldir);

    if(!storage_file_open(entry, filepath, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "open file failed");
        storage_file_free(entry);
        furi_record_close(RECORD_STORAGE);
        return -1;
    }

    const char* header = "order,ax,ay,az,gx,gy,gz\n";
    storage_file_write(entry, header, strlen(header));

    const bool alive = icm42688p_who_am_i_ok(ctx->icm);
    FURI_LOG_I(TAG, "WHO_AM_I check: %s", alive ? "OK (0x47)" : "FAIL");
    furi_check(alive); // Hard crash if registers will be zero

    icm42688p_enable_sensors(ctx->icm);
    icm42688p_accel_config(ctx->icm, AccelFullScale16G, DataRate100Hz);
    icm42688p_gyro_config(ctx->icm, GyroFullScale2000DPS, DataRate100Hz);

    FURI_LOG_I(TAG, "polling %lu samples at 100Hz", ctx->samples);

    char line[128];
    uint32_t captured = 0;

    while(captured < ctx->samples && !ctx->cancel) {
        ICM42688PRawData accel = {0};
        ICM42688PRawData gyro = {0};

        const bool a_ok = icm42688p_read_accel_raw(ctx->icm, &accel);
        const bool g_ok = icm42688p_read_gyro_raw(ctx->icm, &gyro);
        if(!a_ok || !g_ok) {
            FURI_LOG_W(TAG, "sample[%lu]: read failed, skipping", captured);
            furi_delay_ms(10);
            continue;
        }

        ICM42688PScaledData accel_scaled = {0};
        ICM42688PScaledData gyro_scaled = {0};

        icm42688p_apply_scale(&accel, icm42688p_accel_get_full_scale(ctx->icm), &accel_scaled);
        icm42688p_apply_scale(&gyro, icm42688p_gyro_get_full_scale(ctx->icm), &gyro_scaled);

        if(captured < 3) {
            FURI_LOG_I(TAG, "read ok: accel=%d gyro=%d", a_ok, g_ok);
            FURI_LOG_I(
                TAG,
                "sample[%ld]: ax=%f ay=%f az=%f gx=%f gy=%f gz=%f",
                captured,
                (double)accel_scaled.x, (double)accel_scaled.y, (double)accel_scaled.z,
                (double)gyro_scaled.x, (double)gyro_scaled.y, (double)gyro_scaled.z);
            if (captured == 2) {
                FURI_LOG_I(TAG, "sample going silent...");
            }
        }

        int len = snprintf(
            line, sizeof(line), "%ld,%f,%f,%f,%f,%f,%f\n",
            captured,
            (double)accel_scaled.x, (double)accel_scaled.y, (double)accel_scaled.z,
            (double)gyro_scaled.x, (double)gyro_scaled.y, (double)gyro_scaled.z
            );
        storage_file_write(entry, line, len);
        captured++;
        furi_delay_ms(10);
    }

    FURI_LOG_I(TAG, "capture complete: %lu samples", captured);

    storage_file_close(entry);
    storage_file_free(entry);
    furi_record_close(RECORD_STORAGE);

    ctx->done = true;
    return 0;
}

void capture(ICM42688P* imu, uint32_t samples, const char* label) {

    // Wait for any previous capture to complete
    if(cap_ctx.thread) {
        furi_thread_join(cap_ctx.thread);
        furi_thread_free(cap_ctx.thread);
        cap_ctx.thread = NULL;
    }

    if(!imu) {
        FURI_LOG_E(TAG, "IMU is NULL!");
        return;
    }
    FURI_LOG_I(TAG, "IMU valid: %p", (void*)imu);
    
    cap_ctx.icm = imu;
    cap_ctx.samples = samples;
    cap_ctx.label = label;
    cap_ctx.done = false;
    cap_ctx.cancel = false;
    FURI_LOG_I(TAG, "context set");
    
    FURI_LOG_I(TAG, "allocating thread...");
    FuriThread* thread = furi_thread_alloc();
    if(!thread) {
        FURI_LOG_E(TAG, "thread alloc failed");
        return;
    }
    FURI_LOG_I(TAG, "thread allocated: %p", (void*)thread);

    furi_thread_set_context(thread, &cap_ctx);
    
    // Increased stack size from 1024 to 2048 for safety
    furi_thread_set_stack_size(thread, 4096);
    furi_thread_set_priority(thread, FuriThreadPriorityNormal);
    furi_thread_set_callback(thread, capture_thread);

    furi_thread_start(thread);

    cap_ctx.thread = thread;
}

bool capture_is_done(void) {
    return cap_ctx.done;
}

void capture_cancel(void) {
    cap_ctx.cancel = true;
}

void capture_join(void) {
    if(cap_ctx.thread) {
        furi_thread_join(cap_ctx.thread);
        furi_thread_free(cap_ctx.thread);
        cap_ctx.thread = NULL;
    }
}