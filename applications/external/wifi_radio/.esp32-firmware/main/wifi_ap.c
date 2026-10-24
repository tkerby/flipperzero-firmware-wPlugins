#include "wifi_ap.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_ap_get_sta_list.h"
#include "nvs.h"

static const char* TAG = "wifi_ap";
static const char* NVS_NS = "radio";
static const char* NVS_KEY_SUFFIX = "ssid_sfx";

// Only the suffix is stored. The prefix is applied by ssid_build() on every
// use, so even a hand-edited NVS value cannot produce an arbitrary SSID.
static char ap_suffix[WIFI_AP_SSID_MAX + 1] = "";
static char ap_ssid[WIFI_AP_SSID_MAX + 1] = SSID_PREFIX;

static void wifi_load_ssid(void) {
    nvs_handle_t h;
    if(nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(ap_suffix);
        if(nvs_get_str(h, NVS_KEY_SUFFIX, ap_suffix, &len) != ESP_OK) ap_suffix[0] = '\0';
        nvs_close(h);
    }
    ssid_build(ap_suffix, ap_ssid, sizeof(ap_ssid));
}

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    (void)arg;
    (void)base;
    // MAC and reason code both go in the log: a phone that struggles to
    // rejoin leaves a trail of these, and the reason says whether we dropped
    // it (inactivity, auth) or it walked away.
    if(id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* e = data;
        ESP_LOGI(
            TAG,
            "phone joined aid=%d %02x:%02x:%02x:%02x:%02x:%02x",
            e->aid,
            e->mac[0],
            e->mac[1],
            e->mac[2],
            e->mac[3],
            e->mac[4],
            e->mac[5]);
    } else if(id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* e = data;
        ESP_LOGI(
            TAG,
            "phone left aid=%d reason=%u %02x:%02x:%02x:%02x:%02x:%02x",
            e->aid,
            (unsigned)e->reason,
            e->mac[0],
            e->mac[1],
            e->mac[2],
            e->mac[3],
            e->mac[4],
            e->mac[5]);
    }
}

static void wifi_apply_config(void) {
    wifi_config_t cfg = {0};
    size_t len = strlen(ap_ssid);
    if(len > WIFI_AP_SSID_MAX) len = WIFI_AP_SSID_MAX;
    memcpy(cfg.ap.ssid, ap_ssid, len);
    cfg.ap.ssid_len = (uint8_t)len;
    cfg.ap.channel = 1;
    // Open network on purpose: this is a public radio station, and a password
    // would defeat the captive-portal "join and listen" flow.
    cfg.ap.authmode = WIFI_AUTH_OPEN;
    cfg.ap.max_connection = 8;
    cfg.ap.beacon_interval = 100;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
}

void wifi_ap_start(void) {
    wifi_load_ssid();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap(); // gives us 192.168.4.1 + DHCP server

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_apply_config();
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "softAP up: \"%s\" on 192.168.4.1", ap_ssid);
}

void wifi_ap_set_name_suffix(const char* suffix) {
    snprintf(ap_suffix, sizeof(ap_suffix), "%s", suffix ? suffix : "");
    ssid_build(ap_suffix, ap_ssid, sizeof(ap_ssid));

    nvs_handle_t h;
    if(nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, NVS_KEY_SUFFIX, ap_suffix);
        nvs_commit(h);
        nvs_close(h);
    }
    wifi_apply_config();
    ESP_LOGI(TAG, "station renamed to \"%s\"", ap_ssid);
}

void wifi_ap_get_ssid(char* out, size_t out_cap) {
    if(out && out_cap) snprintf(out, out_cap, "%s", ap_ssid);
}

uint8_t wifi_ap_station_count(void) {
    wifi_sta_list_t list;
    if(esp_wifi_ap_get_sta_list(&list) != ESP_OK) return 0;
    return (uint8_t)list.num;
}

bool wifi_ap_mac_for_ip(uint32_t ip, uint8_t mac_out[6]) {
    if(!ip || !mac_out) return false;
    wifi_sta_list_t list;
    if(esp_wifi_ap_get_sta_list(&list) != ESP_OK) return false;
    wifi_sta_mac_ip_list_t pairs;
    if(esp_wifi_ap_get_sta_list_with_ip(&list, &pairs) != ESP_OK) return false;
    for(int i = 0; i < pairs.num; i++) {
        if(pairs.sta[i].ip.addr == ip) {
            memcpy(mac_out, pairs.sta[i].mac, 6);
            return true;
        }
    }
    return false;
}
