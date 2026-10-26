#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEVBOARD_SSID_MAX     33
#define DEVBOARD_MAX_NETWORKS 16
#define DEVBOARD_IP_MAX       16

typedef enum {
    DevboardFirmwareUnknown = 0,
    DevboardFirmwareEspAt,
    DevboardFirmwareGhostEsp,
    DevboardFirmwareMarauder,
    DevboardFirmwareBruce,
    DevboardFirmwareBlackMagic,
} DevboardFirmware;

const char* devboard_firmware_name(DevboardFirmware firmware);

const char* devboard_firmware_hint(DevboardFirmware firmware);

bool devboard_firmware_can_monitor(DevboardFirmware firmware);

DevboardFirmware devboard_detect_cli_firmware(const char* cli_output);

size_t devboard_parse_at_scan(
    const char* response,
    char networks[][DEVBOARD_SSID_MAX],
    size_t max_networks);

size_t devboard_parse_cli_scan(
    const char* response,
    char networks[][DEVBOARD_SSID_MAX],
    size_t max_networks);

bool devboard_cli_join_accepted(const char* response);

bool devboard_cli_find_ipv4(const char* text, char* out, size_t out_size);

bool devboard_cli_reports_disconnected(const char* response);

bool devboard_cli_ping_ok(const char* response);
