#include "../devboard_protocol.h"

#include <stdio.h>
#include <string.h>

static int checks_total = 0;
static int checks_failed = 0;

#define CHECK(condition)                                                \
    do {                                                                \
        checks_total++;                                                 \
        if(!(condition)) {                                              \
            checks_failed++;                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        }                                                               \
    } while(0)

static void test_parse_at_scan(void) {
    char networks[DEVBOARD_MAX_NETWORKS][DEVBOARD_SSID_MAX] = {0};
    const char* response = "\r\n+CWLAP:(3,\"HomeNet\",-45,\"aa:bb:cc:dd:ee:ff\",1,0,3)\r\n"
                           "+CWLAP:(4,\"Cafe Free\",-71,\"11:22:33:44:55:66\",6,0,3)\r\n"
                           "+CWLAP:(3,\"HomeNet\",-48,\"aa:bb:cc:dd:ee:ff\",1,0,3)\r\n"
                           "\r\nOK\r\n";

    const size_t count = devboard_parse_at_scan(response, networks, DEVBOARD_MAX_NETWORKS);

    CHECK(count == 2);
    CHECK(strcmp(networks[0], "HomeNet") == 0);
    CHECK(strcmp(networks[1], "Cafe Free") == 0);
}

static void test_parse_at_scan_failure(void) {
    char networks[DEVBOARD_MAX_NETWORKS][DEVBOARD_SSID_MAX] = {0};

    CHECK(devboard_parse_at_scan("\r\nERROR\r\n", networks, DEVBOARD_MAX_NETWORKS) == 0);
    CHECK(devboard_parse_at_scan(NULL, networks, DEVBOARD_MAX_NETWORKS) == 0);
}

static void test_parse_cli_scan(void) {
    char networks[DEVBOARD_MAX_NETWORKS][DEVBOARD_SSID_MAX] = {0};
    const char* response = "[0] SSID: HomeNet,\r\n"
                           "     RSSI: -45,\r\n"
                           "     Company: TP-Link\r\n"
                           "[1] SSID: Hidden Network,\r\n"
                           "     RSSI: -60,\r\n"
                           "[2] SSID: Cafe Free ,\r\n"
                           "     RSSI: -71,\r\n"
                           "[3] SSID: HomeNet,\r\n"
                           "     RSSI: -48,\r\n"
                           "Scan complete: 4 networks\r\n";

    const size_t count = devboard_parse_cli_scan(response, networks, DEVBOARD_MAX_NETWORKS);

    CHECK(count == 2);
    CHECK(strcmp(networks[0], "HomeNet") == 0);
    CHECK(strcmp(networks[1], "Cafe Free") == 0);
}

static void test_parse_cli_scan_empty(void) {
    char networks[DEVBOARD_MAX_NETWORKS][DEVBOARD_SSID_MAX] = {0};

    CHECK(devboard_parse_cli_scan("Scanning...\r\nDone\r\n", networks, 16) == 0);
    CHECK(devboard_parse_cli_scan(NULL, networks, 16) == 0);
}

static void test_detect_cli_firmware(void) {
    CHECK(
        devboard_detect_cli_firmware("Ghost ESP v2.1.0\r\nhelp\r\n") == DevboardFirmwareGhostEsp);
    CHECK(
        devboard_detect_cli_firmware("commands: startportal, scanaps") ==
        DevboardFirmwareGhostEsp);
    CHECK(devboard_detect_cli_firmware("Bruce v1.9\r\nhelp") == DevboardFirmwareBruce);
    CHECK(devboard_detect_cli_firmware("ESP32 Marauder v0.13.0") == DevboardFirmwareMarauder);
    CHECK(devboard_detect_cli_firmware("scanap stopscan beaconspam") == DevboardFirmwareMarauder);
    CHECK(devboard_detect_cli_firmware("garbage") == DevboardFirmwareUnknown);
    CHECK(devboard_detect_cli_firmware(NULL) == DevboardFirmwareUnknown);
}

static void test_firmware_capabilities(void) {
    CHECK(devboard_firmware_can_monitor(DevboardFirmwareEspAt));
    CHECK(devboard_firmware_can_monitor(DevboardFirmwareGhostEsp));
    CHECK(!devboard_firmware_can_monitor(DevboardFirmwareMarauder));
    CHECK(!devboard_firmware_can_monitor(DevboardFirmwareBruce));
    CHECK(!devboard_firmware_can_monitor(DevboardFirmwareBlackMagic));
    CHECK(!devboard_firmware_can_monitor(DevboardFirmwareUnknown));

    CHECK(strcmp(devboard_firmware_name(DevboardFirmwareEspAt), "ESP-AT") == 0);
    CHECK(strcmp(devboard_firmware_name(DevboardFirmwareGhostEsp), "GhostESP") == 0);
    CHECK(strcmp(devboard_firmware_name(DevboardFirmwareBruce), "Bruce") == 0);
    CHECK(strcmp(devboard_firmware_name(DevboardFirmwareMarauder), "Marauder") == 0);
    CHECK(strcmp(devboard_firmware_name(DevboardFirmwareBlackMagic), "Black Magic") == 0);

    CHECK(strcmp(devboard_firmware_hint(DevboardFirmwareEspAt), "ESP-AT") == 0);
    CHECK(
        strcmp(devboard_firmware_hint(DevboardFirmwareBlackMagic), "Debug probe, no Wi-Fi") == 0);
    CHECK(strcmp(devboard_firmware_hint(DevboardFirmwareUnknown), "Unknown firmware") == 0);
    CHECK(strlen(devboard_firmware_hint(DevboardFirmwareMarauder)) < 24);
    CHECK(strlen(devboard_firmware_hint(DevboardFirmwareBruce)) < 24);
}

static void test_join_accepted(void) {
    CHECK(devboard_cli_join_accepted("Connecting to SSID: HomeNet, please wait..."));
    CHECK(!devboard_cli_join_accepted("error: network not found"));
    CHECK(!devboard_cli_join_accepted(NULL));
}

static void test_find_ipv4(void) {
    char ip[DEVBOARD_IP_MAX] = {0};

    CHECK(devboard_cli_find_ipv4("IP: 192.168.1.42\r\n", ip, sizeof(ip)));
    CHECK(strcmp(ip, "192.168.1.42") == 0);

    CHECK(!devboard_cli_find_ipv4("IP: 0.0.0.0\r\n", ip, sizeof(ip)));
    CHECK(!devboard_cli_find_ipv4("IP: 127.0.0.1\r\n", ip, sizeof(ip)));
    CHECK(!devboard_cli_find_ipv4("no address here", ip, sizeof(ip)));
    CHECK(!devboard_cli_find_ipv4("IP: 999.1.1.1\r\n", ip, sizeof(ip)));
    CHECK(!devboard_cli_find_ipv4(NULL, ip, sizeof(ip)));
}

static void test_reports_disconnected(void) {
    CHECK(devboard_cli_reports_disconnected("Status: not connected"));
    CHECK(devboard_cli_reports_disconnected("disconnected from HomeNet"));
    CHECK(!devboard_cli_reports_disconnected("unknown command: wifistatus"));
    CHECK(!devboard_cli_reports_disconnected("SSID: HomeNet, IP: 192.168.1.42"));
}

static void test_ping(void) {
    CHECK(devboard_cli_ping_ok("64 bytes from 8.8.8.8: icmp_seq=1 ttl=115 time=12.4 ms"));
    CHECK(devboard_cli_ping_ok("Reply from 8.8.8.8: bytes=32 time=13ms TTL=115"));
    CHECK(devboard_cli_ping_ok("1 packets transmitted, 1 packets received"));

    CHECK(!devboard_cli_ping_ok("1 packets transmitted, 0 received, 100% packet loss"));
    CHECK(!devboard_cli_ping_ok("Destination Host Unreachable"));
    CHECK(!devboard_cli_ping_ok("unknown command: ping"));
    CHECK(!devboard_cli_ping_ok("not connected"));
    CHECK(!devboard_cli_ping_ok(NULL));
}

int main(void) {
    test_parse_at_scan();
    test_parse_at_scan_failure();
    test_parse_cli_scan();
    test_parse_cli_scan_empty();
    test_detect_cli_firmware();
    test_firmware_capabilities();
    test_join_accepted();
    test_find_ipv4();
    test_reports_disconnected();
    test_ping();

    printf("%d checks, %d failed\n", checks_total, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}
