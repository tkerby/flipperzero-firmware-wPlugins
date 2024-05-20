#include "../cfw_app.h"

enum VarItemListIndex {
    VarItemListIndexSubghzFrequencies,
    VarItemListIndexSubghzExtend,
    VarItemListIndexSubghzBypass,
    VarItemListIndexSpiCc1101Handle,
    VarItemListIndexSpiNrf24Handle,
    VarItemListIndexUartEspChannel,
    VarItemListIndexUartNmeaChannel,
};

#define SPI_DEFAULT "Default 4"
#define SPI_EXTRA "Extra 7"
#define UART_DEFAULT "Default 13,14"
#define UART_EXTRA "Extra 15,16"

void cfw_app_scene_protocols_var_item_list_callback(void* context, uint32_t index) {
    CfwApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cfw_app_scene_protocols_subghz_extend_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    app->subghz_extend = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, app->subghz_extend ? "ON" : "OFF");
    app->save_subghz = true;
    app->require_reboot = true;
}

static void cfw_app_scene_protocols_subghz_bypass_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    app->subghz_bypass = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, app->subghz_bypass ? "ON" : "OFF");
    app->save_subghz = true;
    app->require_reboot = true;
}

static void cfw_app_scene_protocols_cc1101_handle_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    cfw_settings.spi_cc1101_handle =
        variable_item_get_current_value_index(item) == 0 ? SpiDefault : SpiExtra;
    variable_item_set_current_value_text(
        item, cfw_settings.spi_cc1101_handle == SpiDefault ? SPI_DEFAULT : SPI_EXTRA);
    app->save_settings = true;
}

static void cfw_app_scene_protocols_nrf24_handle_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    cfw_settings.spi_nrf24_handle = variable_item_get_current_value_index(item) == 0 ? SpiDefault :
                                                                                       SpiExtra;
    variable_item_set_current_value_text(
        item, cfw_settings.spi_nrf24_handle == SpiDefault ? SPI_DEFAULT : SPI_EXTRA);
    app->save_settings = true;
}

static void cfw_app_scene_protocols_esp32_channel_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    cfw_settings.uart_esp_channel = variable_item_get_current_value_index(item) == 0 ?
                                        FuriHalSerialIdUsart :
                                        FuriHalSerialIdLpuart;
    variable_item_set_current_value_text(
        item, cfw_settings.uart_esp_channel == FuriHalSerialIdUsart ? UART_DEFAULT : UART_EXTRA);
    app->save_settings = true;
}

static void cfw_app_scene_protocols_nmea_channel_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    cfw_settings.uart_nmea_channel = variable_item_get_current_value_index(item) == 0 ?
                                         FuriHalSerialIdUsart :
                                         FuriHalSerialIdLpuart;
    variable_item_set_current_value_text(
        item, cfw_settings.uart_nmea_channel == FuriHalSerialIdUsart ? UART_DEFAULT : UART_EXTRA);
    app->save_settings = true;
}

void cfw_app_scene_protocols_on_enter(void* context) {
    CfwApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    variable_item_list_add(var_item_list, "SubGHz Frequencies", 0, NULL, app);

    item = variable_item_list_add(
        var_item_list, "SubGHz Extend", 2, cfw_app_scene_protocols_subghz_extend_changed, app);
    variable_item_set_current_value_index(item, app->subghz_extend);
    variable_item_set_current_value_text(item, app->subghz_extend ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list, "SubGHz Bypass", 2, cfw_app_scene_protocols_subghz_bypass_changed, app);
    variable_item_set_current_value_index(item, app->subghz_bypass);
    variable_item_set_current_value_text(item, app->subghz_bypass ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list, "SPI CC1101 Handle", 2, cfw_app_scene_protocols_cc1101_handle_changed, app);
    variable_item_set_current_value_index(item, cfw_settings.spi_cc1101_handle);
    variable_item_set_current_value_text(
        item, cfw_settings.spi_cc1101_handle == SpiDefault ? SPI_DEFAULT : SPI_EXTRA);

    item = variable_item_list_add(
        var_item_list, "SPI NRF24 Handle", 2, cfw_app_scene_protocols_nrf24_handle_changed, app);
    variable_item_set_current_value_index(item, cfw_settings.spi_nrf24_handle);
    variable_item_set_current_value_text(
        item, cfw_settings.spi_nrf24_handle == SpiDefault ? SPI_DEFAULT : SPI_EXTRA);

    item = variable_item_list_add(
        var_item_list, "ESP32/ESP8266 UART", 2, cfw_app_scene_protocols_esp32_channel_changed, app);
    variable_item_set_current_value_index(item, cfw_settings.uart_esp_channel);
    variable_item_set_current_value_text(
        item, cfw_settings.uart_esp_channel == FuriHalSerialIdUsart ? UART_DEFAULT : UART_EXTRA);

    item = variable_item_list_add(
        var_item_list, "NMEA GPS UART", 2, cfw_app_scene_protocols_nmea_channel_changed, app);
    variable_item_set_current_value_index(item, cfw_settings.uart_nmea_channel);
    variable_item_set_current_value_text(
        item, cfw_settings.uart_nmea_channel == FuriHalSerialIdUsart ? UART_DEFAULT : UART_EXTRA);

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_protocols_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, CfwAppSceneProtocols));

    view_dispatcher_switch_to_view(app->view_dispatcher, CfwAppViewVarItemList);
}

bool cfw_app_scene_protocols_on_event(void* context, SceneManagerEvent event) {
    CfwApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, CfwAppSceneProtocols, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexSubghzFrequencies:
            scene_manager_set_scene_state(app->scene_manager, CfwAppSceneProtocolsFrequencies, 0);
            scene_manager_next_scene(app->scene_manager, CfwAppSceneProtocolsFrequencies);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_protocols_on_exit(void* context) {
    CfwApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
