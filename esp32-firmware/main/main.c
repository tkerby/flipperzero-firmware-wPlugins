// Flipper Radio — ESP32-S2 firmware entry point.
//
// Wires up the four subsystems: WiFi softAP, captive-portal DNS, HTTP server
// (player page + live MP3 stream), and the UART link to the Flipper.
//
// Phase 0: everything is a stub that logs. Phase 1 fills in wifi_ap + dns +
// http (serving an embedded MP3 loop). Phase 2 adds the UART audio source.

#include "esp_log.h"
#include "nvs_flash.h"

#include "status_led.h"
#include "stream_hub.h"
#include "wifi_ap.h"
#include "dns_captive.h"
#include "http_srv.h"
#include "uart_link.h"

static const char *TAG = "radio";

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // First, before anything that can fail or take time: until this runs the
    // board is indistinguishable from a dead one, which is exactly the thing
    // the LED exists to prevent.
    status_led_start();

    ESP_LOGI(TAG, "Flipper Radio booting");
    stream_hub_init();    // must exist before any audio source or listener
    wifi_ap_start();      // softAP "Flipper Radio"
    dns_captive_start();  // answer all lookups with our IP
    http_srv_start();     // /, /stream, /status
    uart_link_start();    // Flipper audio source + stats
    ESP_LOGI(TAG, "startup complete");
}
