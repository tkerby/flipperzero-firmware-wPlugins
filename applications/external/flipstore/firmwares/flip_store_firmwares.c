#include <firmwares/flip_store_firmwares.h>

Firmware* firmwares = NULL;
VGMFirmware* vgm_firmwares = NULL;
bool is_esp32_firmware = true;

Firmware* firmware_alloc() {
    Firmware* fw = (Firmware*)malloc(FIRMWARE_COUNT * sizeof(Firmware));
    if(!fw) {
        FURI_LOG_E(TAG, "Failed to allocate memory for Firmware");
        return NULL;
    }

    // Black Magic
    snprintf(fw[0].name, sizeof(fw[0].name), "%s", "Black Magic");
    snprintf(
        fw[0].links[0],
        sizeof(fw[0].links[0]),
        "%s",
        "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/BM/bootloader.bin");
    snprintf(
        fw[0].links[1],
        sizeof(fw[0].links[1]),
        "%s",
        "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/BM/partition-table.bin");
    snprintf(
        fw[0].links[2],
        sizeof(fw[0].links[2]),
        "%s",
        "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/BM/blackmagic.bin");

    // FlipperHTTP
    snprintf(fw[1].name, sizeof(fw[1].name), "%s", "FlipperHTTP");
    snprintf(
        fw[1].links[0],
        sizeof(fw[1].links[0]),
        "%s",
        "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/WiFi%20Developer%20Board%20(ESP32S2)/flipper_http_bootloader.bin");
    snprintf(
        fw[1].links[1],
        sizeof(fw[1].links[1]),
        "%s",
        "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/WiFi%20Developer%20Board%20(ESP32S2)/flipper_http_firmware_a.bin");
    snprintf(
        fw[1].links[2],
        sizeof(fw[1].links[2]),
        "%s",
        "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/WiFi%20Developer%20Board%20(ESP32S2)/flipper_http_partitions.bin");

    // Marauder (this changes too often.. we need a static link for the third one. maybe I'll fork the repo and host it myself)
    snprintf(fw[2].name, sizeof(fw[2].name), "%s", "Marauder");
    snprintf(
        fw[2].links[0],
        sizeof(fw[2].links[0]),
        "%s",
        "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/M/FLIPDEV/esp32_marauder.ino.bootloader.bin");
    snprintf(
        fw[2].links[1],
        sizeof(fw[2].links[1]),
        "%s",
        "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/M/FLIPDEV/esp32_marauder.ino.partitions.bin");
    snprintf(
        fw[2].links[2],
        sizeof(fw[2].links[2]),
        "%s",
        "https://raw.githubusercontent.com/jblanked/fzeeflasher.github.io/main/resources/CURRENT/esp32_marauder_v1_4_1_20250406_flipper.bin");

    return fw;
}

VGMFirmware* vgm_firmware_alloc() {
    VGMFirmware* fw = (VGMFirmware*)malloc(VGM_FIRMWARE_COUNT * sizeof(VGMFirmware));
    if(!fw) {
        FURI_LOG_E(TAG, "Failed to allocate memory for VGM Firmware");
        return NULL;
    }

    // FlipperHTTP
    snprintf(fw[0].name, sizeof(fw[0].name), "%s", "FlipperHTTP");
    snprintf(
        fw[0].link,
        sizeof(fw[0].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/Video%20Game%20Module/MicroPython/flipper_http_vgm_micro_python.uf2");

    /* VGM Library specs
    Screensavers: https://github.com/jblanked/VGM-Library/tree/main/Screensavers
        - aquarium.uf2
        - boing_ball.uf2
        - bouncing_balls.uf2
        - dvi_logo_bounce.uf2
        - flying_toasters.uf2
        - trippy_tvhost.uf2
    Games: https://github.com/jblanked/VGM-Library/tree/main/engine/Arduino
        - AirLabyrinth-VGM-Engine.uf2
        - Arkanoid-VGM-Engine.uf2
        - Doom-VGM-Engine.uf2
        - Doom_8bit-VGM-Engine.uf2
        - FlappyBird-VGM-Engine.uf2
        - FlightAssault-VGM-Engine.uf2
        - FlipWorld-VGM-Engine.uf2
        - FuriousBirds-VGM-Engine.uf2
        - Hawaii-VGM-Engine.uf2
        - Pong-VGM-Engine.uf2
        - T-Rex-Runner-VGM-Engine.uf2
        - Tetris-VGM-Engine.uf2
        - example_8bit-VGM-Engine.uf2
    */
    // VGM Library
    snprintf(fw[1].name, sizeof(fw[1].name), "%s", "Aquarium - Screensaver");
    snprintf(
        fw[1].link,
        sizeof(fw[1].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/Screensavers/aquarium.uf2");
    //
    snprintf(fw[2].name, sizeof(fw[2].name), "%s", "Boing Ball - Screensaver");
    snprintf(
        fw[2].link,
        sizeof(fw[2].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/Screensavers/boing_ball.uf2");
    //
    snprintf(fw[3].name, sizeof(fw[3].name), "%s", "Bouncing Balls - Screensaver");
    snprintf(
        fw[3].link,
        sizeof(fw[3].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/Screensavers/bouncing_balls.uf2");
    //
    snprintf(fw[4].name, sizeof(fw[4].name), "%s", "DVI Logo Bounce - Screensaver");
    snprintf(
        fw[4].link,
        sizeof(fw[4].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/Screensavers/dvi_logo_bounce.uf2");
    //
    snprintf(fw[5].name, sizeof(fw[5].name), "%s", "Flying Toasters - Screensaver");
    snprintf(
        fw[5].link,
        sizeof(fw[5].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/Screensavers/flying_toasters.uf2");
    //
    snprintf(fw[6].name, sizeof(fw[6].name), "%s", "Trippy TV Host - Screensaver");
    snprintf(
        fw[6].link,
        sizeof(fw[6].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/Screensavers/trippy_tvhost.uf2");
    //
    snprintf(fw[7].name, sizeof(fw[7].name), "%s", "Air Labyrinth - Game");
    snprintf(
        fw[7].link,
        sizeof(fw[7].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/AirLabyrinth-VGM-Engine.uf2");
    //
    snprintf(fw[8].name, sizeof(fw[8].name), "%s", "Arkanoid - Game");
    snprintf(
        fw[8].link,
        sizeof(fw[8].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/Arkanoid-VGM-Engine.uf2");
    //
    snprintf(fw[9].name, sizeof(fw[9].name), "%s", "Doom - Game");
    snprintf(
        fw[9].link,
        sizeof(fw[9].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/Doom-VGM-Engine.uf2");
    //
    snprintf(fw[10].name, sizeof(fw[10].name), "%s", "Doom 8bit - Game");
    snprintf(
        fw[10].link,
        sizeof(fw[10].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/Doom_8bit-VGM-Engine.uf2");
    //
    snprintf(fw[11].name, sizeof(fw[11].name), "%s", "Flappy Bird - Game");
    snprintf(
        fw[11].link,
        sizeof(fw[11].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/FlappyBird-VGM-Engine.uf2");
    //
    snprintf(fw[12].name, sizeof(fw[12].name), "%s", "Flight Assault - Game");
    snprintf(
        fw[12].link,
        sizeof(fw[12].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/FlightAssault-VGM-Engine.uf2");
    //
    snprintf(fw[13].name, sizeof(fw[13].name), "%s", "Flip World - Game");
    snprintf(
        fw[13].link,
        sizeof(fw[13].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/FlipWorld-VGM-Engine.uf2");
    //
    snprintf(fw[14].name, sizeof(fw[14].name), "%s", "Furious Birds - Game");
    snprintf(
        fw[14].link,
        sizeof(fw[14].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/FuriousBirds-VGM-Engine.uf2");
    //
    snprintf(fw[15].name, sizeof(fw[15].name), "%s", "Hawaii - Game");
    snprintf(
        fw[15].link,
        sizeof(fw[15].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/Hawaii-VGM-Engine.uf2");
    //
    snprintf(fw[16].name, sizeof(fw[16].name), "%s", "Pong - Game");
    snprintf(
        fw[16].link,
        sizeof(fw[16].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/Pong-VGM-Engine.uf2");
    //
    snprintf(fw[17].name, sizeof(fw[17].name), "%s", "T-Rex Runner - Game");
    snprintf(
        fw[17].link,
        sizeof(fw[17].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/T-Rex-Runner-VGM-Engine.uf2");
    //
    snprintf(fw[18].name, sizeof(fw[18].name), "%s", "Tetris - Game");
    snprintf(
        fw[18].link,
        sizeof(fw[18].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/Tetris-VGM-Engine.uf2");
    //
    snprintf(fw[19].name, sizeof(fw[19].name), "%s", "Example 8bit - Game");
    snprintf(
        fw[19].link,
        sizeof(fw[19].link),
        "%s",
        "https://raw.githubusercontent.com/jblanked/VGM-Library/main/engine/Arduino/example_8bit-VGM-Engine.uf2");

    return fw;
}

void firmware_free() {
    if(firmwares) {
        free(firmwares);
        firmwares = NULL;
    }
}
void vgm_firmware_free() {
    if(vgm_firmwares) {
        free(vgm_firmwares);
        vgm_firmwares = NULL;
    }
}

bool flip_store_get_firmware_file(FlipperHTTP* fhttp, char* link, char* name, char* filename) {
    if(!fhttp) {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    if(fhttp->state == INACTIVE) {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);

    char directory_path[64];
    // save in ESP32 flasher directory
    if(is_esp32_firmware) {
        snprintf(
            directory_path,
            sizeof(directory_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher");
        storage_common_mkdir(storage, directory_path);
        snprintf(
            directory_path,
            sizeof(directory_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/%s",
            firmwares[selected_firmware_index].name);
        storage_common_mkdir(storage, directory_path);
        snprintf(
            fhttp->file_path,
            sizeof(fhttp->file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/%s/%s",
            name,
            filename);
    } else // install in app_data directory
    {
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/vgm");
        storage_common_mkdir(storage, directory_path);
        snprintf(
            fhttp->file_path,
            sizeof(fhttp->file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/vgm/%s",
            filename);
    }
    furi_record_close(RECORD_STORAGE);
    fhttp->save_received_data = false;
    fhttp->is_bytes_request = true;
    // return flipper_http_get_request_bytes(fhttp, link, "{\"Content-Type\":\"application/octet-stream\"}");
    return flipper_http_request(
        fhttp, BYTES, link, "{\"Content-Type\":\"application/octet-stream\"}", NULL);
}
