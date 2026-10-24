#include "devboard_protocol.h"

#include <string.h>

static char devboard_tolower(char value) {
    if(value >= 'A' && value <= 'Z') return (char)(value + ('a' - 'A'));
    return value;
}

static const char*
    devboard_find_ci_n(const char* haystack, size_t haystack_length, const char* needle) {
    if(!haystack || !needle || needle[0] == '\0') return NULL;

    const size_t needle_length = strlen(needle);
    if(needle_length > haystack_length) return NULL;

    for(size_t offset = 0; offset + needle_length <= haystack_length; offset++) {
        size_t matched = 0;
        while(matched < needle_length &&
              devboard_tolower(haystack[offset + matched]) == devboard_tolower(needle[matched])) {
            matched++;
        }
        if(matched == needle_length) return haystack + offset;
    }

    return NULL;
}

static bool devboard_contains_ci(const char* haystack, const char* needle) {
    if(!haystack) return false;
    return devboard_find_ci_n(haystack, strlen(haystack), needle) != NULL;
}

static void devboard_add_network(
    char networks[][DEVBOARD_SSID_MAX],
    size_t* count,
    size_t max_networks,
    const char* ssid,
    size_t ssid_length) {
    if(!ssid || *count >= max_networks || ssid_length == 0 || ssid_length >= DEVBOARD_SSID_MAX) {
        return;
    }

    for(size_t i = 0; i < *count; i++) {
        if(strlen(networks[i]) == ssid_length && memcmp(networks[i], ssid, ssid_length) == 0) {
            return;
        }
    }

    memcpy(networks[*count], ssid, ssid_length);
    networks[*count][ssid_length] = '\0';
    (*count)++;
}

static bool devboard_ip_is_usable(const char* ip) {
    if(strncmp(ip, "0.", 2) == 0) return false;
    if(strncmp(ip, "127.", 4) == 0) return false;
    if(strncmp(ip, "255.", 4) == 0) return false;
    return true;
}

const char* devboard_firmware_name(DevboardFirmware firmware) {
    switch(firmware) {
    case DevboardFirmwareEspAt:
        return "ESP-AT";
    case DevboardFirmwareGhostEsp:
        return "GhostESP";
    case DevboardFirmwareMarauder:
        return "Marauder";
    case DevboardFirmwareBruce:
        return "Bruce";
    case DevboardFirmwareBlackMagic:
        return "Black Magic";
    case DevboardFirmwareUnknown:
    default:
        return "Unknown";
    }
}

const char* devboard_firmware_hint(DevboardFirmware firmware) {
    switch(firmware) {
    case DevboardFirmwareBlackMagic:
        return "Debug probe, no Wi-Fi";
    case DevboardFirmwareMarauder:
        return "Marauder: no Wi-Fi join";
    case DevboardFirmwareBruce:
        return "Bruce: no Wi-Fi join";
    case DevboardFirmwareUnknown:
        return "Unknown firmware";
    default:
        return devboard_firmware_name(firmware);
    }
}

bool devboard_firmware_can_monitor(DevboardFirmware firmware) {
    return firmware == DevboardFirmwareEspAt || firmware == DevboardFirmwareGhostEsp;
}

DevboardFirmware devboard_detect_cli_firmware(const char* cli_output) {
    if(!cli_output || cli_output[0] == '\0') return DevboardFirmwareUnknown;

    if(devboard_contains_ci(cli_output, "ghostesp") ||
       devboard_contains_ci(cli_output, "ghost esp") ||
       devboard_contains_ci(cli_output, "ghostnet") ||
       devboard_contains_ci(cli_output, "startportal")) {
        return DevboardFirmwareGhostEsp;
    }

    if(devboard_contains_ci(cli_output, "bruce")) return DevboardFirmwareBruce;
    if(devboard_contains_ci(cli_output, "marauder")) return DevboardFirmwareMarauder;

    if(devboard_contains_ci(cli_output, "scanap") ||
       devboard_contains_ci(cli_output, "stopscan") ||
       devboard_contains_ci(cli_output, "beaconspam")) {
        return DevboardFirmwareMarauder;
    }

    return DevboardFirmwareUnknown;
}

size_t devboard_parse_at_scan(
    const char* response,
    char networks[][DEVBOARD_SSID_MAX],
    size_t max_networks) {
    if(!response || max_networks == 0) return 0;

    size_t count = 0;
    const char* cursor = response;

    while(count < max_networks) {
        const char* entry = strstr(cursor, "+CWLAP:");
        if(!entry) break;

        const char* start = strchr(entry, '"');
        if(!start) break;
        start++;

        const char* end = strchr(start, '"');
        if(!end) break;

        devboard_add_network(networks, &count, max_networks, start, (size_t)(end - start));
        cursor = end + 1;
    }

    return count;
}

size_t devboard_parse_cli_scan(
    const char* response,
    char networks[][DEVBOARD_SSID_MAX],
    size_t max_networks) {
    if(!response || max_networks == 0) return 0;

    size_t count = 0;
    const char* cursor = response;

    while(cursor[0] != '\0' && count < max_networks) {
        const char* line_end = strchr(cursor, '\n');
        const size_t line_length = line_end ? (size_t)(line_end - cursor) : strlen(cursor);

        const char* marker = devboard_find_ci_n(cursor, line_length, "ssid:");
        if(marker) {
            const char* value = marker + 5;
            size_t value_length = line_length - (size_t)(value - cursor);

            while(value_length > 0 && (value[0] == ' ' || value[0] == '\t')) {
                value++;
                value_length--;
            }
            while(value_length > 0 &&
                  (value[value_length - 1] == ' ' || value[value_length - 1] == '\t' ||
                   value[value_length - 1] == ',' || value[value_length - 1] == '\r')) {
                value_length--;
            }

            const bool hidden = value_length == strlen("Hidden Network") &&
                                devboard_find_ci_n(value, value_length, "Hidden Network") == value;
            if(!hidden && value_length > 0) {
                devboard_add_network(networks, &count, max_networks, value, value_length);
            }
        }

        if(!line_end) break;
        cursor = line_end + 1;
    }

    return count;
}

bool devboard_cli_join_accepted(const char* response) {
    return devboard_contains_ci(response, "connecting to ssid");
}

bool devboard_cli_find_ipv4(const char* text, char* out, size_t out_size) {
    if(!text) return false;

    for(const char* cursor = text; cursor[0] != '\0'; cursor++) {
        if(cursor[0] < '0' || cursor[0] > '9') continue;

        // Skip candidates that are part of a longer dotted address, otherwise a
        // rejected address such as 127.0.0.1 would match again as 27.0.0.1.
        if(cursor > text) {
            const char previous = cursor[-1];
            if((previous >= '0' && previous <= '9') || previous == '.') continue;
        }

        const char* scan = cursor;
        int parts = 0;
        bool valid = true;

        while(parts < 4) {
            int digits = 0;
            int value = 0;
            while(scan[0] >= '0' && scan[0] <= '9') {
                value = value * 10 + (scan[0] - '0');
                if(value > 255) {
                    valid = false;
                    break;
                }
                scan++;
                digits++;
            }
            if(!valid || digits == 0) {
                valid = false;
                break;
            }
            parts++;
            if(parts < 4) {
                if(scan[0] != '.') {
                    valid = false;
                    break;
                }
                scan++;
            }
        }

        if(valid && (scan[0] == '.' || (scan[0] >= '0' && scan[0] <= '9'))) {
            valid = false;
        }

        if(valid && parts == 4) {
            const size_t length = (size_t)(scan - cursor);
            if(length + 1 <= out_size && length < DEVBOARD_IP_MAX) {
                char ip[DEVBOARD_IP_MAX];
                memcpy(ip, cursor, length);
                ip[length] = '\0';
                if(devboard_ip_is_usable(ip)) {
                    if(out) {
                        memcpy(out, ip, length + 1);
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

bool devboard_cli_reports_disconnected(const char* response) {
    if(!response) return true;
    if(devboard_contains_ci(response, "unknown command") ||
       devboard_contains_ci(response, "not supported")) {
        return false;
    }
    return devboard_contains_ci(response, "not connected") ||
           devboard_contains_ci(response, "disconnected") ||
           devboard_contains_ci(response, "no connection") ||
           devboard_contains_ci(response, "station is not");
}

bool devboard_cli_ping_ok(const char* response) {
    if(!response) return false;

    if(devboard_contains_ci(response, "0 received") ||
       devboard_contains_ci(response, "0 packets received") ||
       devboard_contains_ci(response, "100% packet loss") ||
       devboard_contains_ci(response, "unreachable") ||
       devboard_contains_ci(response, "unknown command") ||
       devboard_contains_ci(response, "not connected")) {
        return false;
    }

    return devboard_contains_ci(response, "bytes from") ||
           devboard_contains_ci(response, "reply from") ||
           devboard_contains_ci(response, "time=") ||
           devboard_contains_ci(response, "1 received") ||
           devboard_contains_ci(response, "1 packets received");
}
