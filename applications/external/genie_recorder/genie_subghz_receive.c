#include "genie_subghz_receive.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "GenieSubGHzReceive"

const SubGhzProtocol* subghz_protocol_registry_item_genie[] = {
    &subghz_protocol_genie,
};

const SubGhzProtocolRegistry subghz_protocol_registry_genie = {
    .items = subghz_protocol_registry_item_genie,
    .size = COUNT_OF(subghz_protocol_registry_item_genie)};

static SubGhzEnvironment* load_environment() {
    SubGhzEnvironment* environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(environment, (void*)&subghz_protocol_registry_genie);
    return environment;
}

GenieSubGhz* genie_subghz_alloc() {
    GenieSubGhz* subghz = malloc(sizeof(GenieSubGhz));
    subghz->status = SUBGHZ_RECEIVER_UNINITIALIZED;
    subghz->environment = load_environment();
    subghz->stream = furi_stream_buffer_alloc(sizeof(LevelDuration) * 1024, sizeof(LevelDuration));
    furi_check(subghz->stream);
    subghz->overrun = false;
    return subghz;
}

void genie_subghz_free(GenieSubGhz* subghz) {
    subghz_environment_free(subghz->environment);
    furi_stream_buffer_free(subghz->stream);
    free(subghz);
}

static void
    rx_callback(SubGhzReceiver* receiver, SubGhzProtocolDecoderBase* decoder_base, void* cxt) {
    GenieSubGhz* context = (GenieSubGhz*)cxt;
    FuriString* buffer = furi_string_alloc();
    subghz_protocol_decoder_base_get_string(decoder_base, buffer);
    subghz_receiver_reset(receiver);
    if(context->callback) {
        context->callback(buffer, context->callback_context);
    }
    furi_string_free(buffer);
}

static void rx_capture_callback(bool level, uint32_t duration, void* context) {
    GenieSubGhz* instance = context;

    LevelDuration level_duration = level_duration_make(level, duration);
    if(instance->overrun) {
        instance->overrun = false;
        level_duration = level_duration_reset();
    }
    size_t ret =
        furi_stream_buffer_send(instance->stream, &level_duration, sizeof(LevelDuration), 0);
    if(sizeof(LevelDuration) != ret) {
        instance->overrun = true;
    }
}

static int32_t listen_rx(void* ctx) {
    GenieSubGhz* context = (GenieSubGhz*)ctx;
    context->status = SUBGHZ_RECEIVER_LISTENING;
    LevelDuration level_duration;
    FURI_LOG_I(TAG, "listen_rx started...");
    while(context->status == SUBGHZ_RECEIVER_LISTENING) {
        int ret = furi_stream_buffer_receive(
            context->stream, &level_duration, sizeof(LevelDuration), 10);

        if(ret == sizeof(LevelDuration)) {
            if(level_duration_is_reset(level_duration)) {
                subghz_receiver_reset(context->receiver);
            } else {
                bool level = level_duration_get_level(level_duration);
                uint32_t duration = level_duration_get_duration(level_duration);
                subghz_receiver_decode(context->receiver, level, duration);
            }
        }
    }
    FURI_LOG_I(TAG, "listen_rx exiting...");
    context->status = SUBGHZ_RECEIVER_NOTLISTENING;
    return 0;
}

void start_listening(
    GenieSubGhz* context,
    uint32_t frequency,
    SubghzPacketCallback callback,
    void* callback_context) {
    context->status = SUBGHZ_RECEIVER_INITIALIZING;

    context->callback = callback;
    context->callback_context = callback_context;
    subghz_devices_init();
    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!subghz_devices_is_frequency_valid(device, frequency)) {
        FURI_LOG_E(TAG, "Frequency not in range. %lu\r\n", frequency);
        subghz_devices_deinit();
        return;
    }

    context->receiver = subghz_receiver_alloc_init(context->environment);
    subghz_receiver_set_filter(context->receiver, SubGhzProtocolFlag_Decodable);
    subghz_receiver_set_rx_callback(context->receiver, rx_callback, context);

    subghz_devices_begin(device);
    subghz_devices_reset(device);
    subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);
    frequency = subghz_devices_set_frequency(device, frequency);

    furi_hal_power_suppress_charge_enter();

    subghz_devices_start_async_rx(device, rx_capture_callback, context);

    FURI_LOG_I(TAG, "Listening at frequency: %lu\r\n", frequency);

    // This thread name is used by the genie protocol decoder to determine if it is running in the
    // context of this application.  If it is, it will not attempt to find the next code.
    context->thread = furi_thread_alloc_ex("genie-rx", 1024, listen_rx, context);
    furi_thread_start(context->thread);
}

void stop_listening(GenieSubGhz* context) {
    if(context->status == SUBGHZ_RECEIVER_UNINITIALIZED) {
        return;
    }

    context->status = SUBGHZ_RECEIVER_UNINITIALING;
    FURI_LOG_D(TAG, "Stopping listening...");
    furi_thread_join(context->thread);
    furi_thread_free(context->thread);
    furi_hal_power_suppress_charge_exit();

    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    subghz_devices_stop_async_rx(device);

    subghz_receiver_free(context->receiver);
    subghz_devices_deinit();
    context->status = SUBGHZ_RECEIVER_UNINITIALIZED;
}