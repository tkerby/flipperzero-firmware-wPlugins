#ifndef _SUBGHZ_MODULE_CLASS_
#define _SUBGHZ_MODULE_CLASS_

#include <furi.h>
#include <furi_hal.h>

#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>

#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
// #include <lib/subghz/protocols/protocol_items.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_configs.h>

#include <lib/subghz/subghz_protocol_registry.h>

#include "lib/HandlerContext.cpp"

#undef LOG_TAG
#define LOG_TAG "SUB_GHZ"

class SubGhzModule {
private:
    SubGhzEnvironment* environment;
    const SubGhzDevice* device;
    SubGhzReceiver* receiver;

    bool isExternal;
    uint32_t frequency = 433920000;

    static void receiveCallback(bool level, uint32_t duration, void* context) {
        UNUSED(level);
        UNUSED(duration);
        UNUSED(context);
    }

    static void captureCallback(SubGhzReceiver* receiver, SubGhzProtocolDecoderBase* decoderBase, void* context) {
        if(context == NULL) {
            FURI_LOG_W(LOG_TAG, "SubGhz module has NULL receive handler!");
            return;
        }

        FuriString* text = furi_string_alloc();
        subghz_protocol_decoder_base_get_string(decoderBase, text);
        subghz_receiver_reset(receiver);
        printf("%s", furi_string_get_cstr(text));
        furi_string_free(text);

        HandlerContext<function<void()>>* handlerContext = (HandlerContext<function<void()>>*)context;
        handlerContext->GetHandler()();
    }

public:
    SubGhzModule() {
        environment = subghz_environment_alloc();
        subghz_environment_set_protocol_registry(environment, &subghz_protocol_registry);

        subghz_devices_init();
        furi_hal_power_enable_otg();
        device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
        if(!subghz_devices_is_connect(device)) {
            furi_hal_power_disable_otg();
            device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
            isExternal = false;
        } else {
            isExternal = true;
        }

        subghz_devices_begin(device);
        subghz_devices_reset(device);
        subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);

        SetFrequency(frequency);

        receiver = subghz_receiver_alloc_init(environment);
        subghz_receiver_set_filter(receiver, SubGhzProtocolFlag_Decodable);
    }

    uint32_t SetFrequency(uint32_t frequency) {
        return subghz_devices_set_frequency(device, frequency);
    }

    void SetReceiveHandler(function<void()> handler) {
        subghz_receiver_set_rx_callback(receiver, captureCallback, new HandlerContext(handler));
    }

    void ReceiveAsync() {
        subghz_devices_start_async_rx(device, (void*)receiveCallback, NULL);
    }

    ~SubGhzModule() {
        if(furi_hal_power_is_otg_enabled()) {
            furi_hal_power_disable_otg();
        }

        if(receiver != NULL) {
            subghz_receiver_free(receiver);
            receiver = NULL;
        }

        if(environment != NULL) {
            subghz_environment_free(environment);
            environment = NULL;
        }

        if(device != NULL) {
            subghz_devices_end(device);
            subghz_devices_deinit();
            device = NULL;
        }
    }
};

#endif //_SUBGHZ_MODULE_CLASS_
