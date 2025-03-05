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

class SubGhzModule {
private:
    SubGhzEnvironment* environment;
    const SubGhzDevice* device;
    bool isExternal;

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
    }

    ~SubGhzModule() {
        if(furi_hal_power_is_otg_enabled()) {
            furi_hal_power_disable_otg();
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
