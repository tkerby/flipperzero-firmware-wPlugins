#pragma once
#include <Arduino.h>

/* Pico Calc
#define VGMGameEngine_SCK 10  //
#define VGMGameEngine_MOSI 11 // TX
#define VGMGameEngine_MISO 12 // RX
#define VGMGameEngine_CS 13   //
#define VGMGameEngine_DC 14
#define VGMGameEngine_RST 15
*/

/* VGM
#define VGMGameEngine_SCK 8
#define VGMGameEngine_MOSI 11
#define VGMGameEngine_MISO 12
#define VGMGameEngine_CS 13
#define VGMGameEngine_DC 14
#define VGMGameEngine_RST 15

static const struct dvi_serialiser_cfg picodvi_dvi_cfg = {
    .pio = DVI_DEFAULT_PIO_INST,
    .sm_tmds = {0, 1, 2},
    .pins_tmds = {10, 12, 14},
    .pins_clk = 8,
    .invert_diffpairs = true
};

*/

// additional colors
// https://doc-tft-espi.readthedocs.io/tft_espi/colors/
#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif

#ifndef TFT_RED
#define TFT_RED 0xF800
#endif

#ifndef TFT_WHITE
#define TFT_WHITE 0xFFFF
#endif

namespace VGMGameEngine
{
    typedef enum
    {
        BOARD_TYPE_PICO_CALC = 0,
        BOARD_TYPE_VGM = 1,
        BOARD_TYPE_JBLANKED = 2,
    } BoardType;

    typedef enum
    {
        PICO_TYPE_PICO = 0,    // Raspberry Pi Pico
        PICO_TYPE_PICO_W = 1,  // Raspberry Pi Pico W
        PICO_TYPE_PICO_2 = 2,  // Raspberry Pi Pico 2
        PICO_TYPE_PICO_2_W = 3 // Raspberry Pi Pico 2 W
    } PicoType;

    typedef enum
    {
        LIBRARY_TYPE_PICO_DVI = 0,
        LIBRARY_TYPE_TFT = 1
    } LibraryType;

    typedef struct
    {
        uint8_t sck;
        uint8_t mosi;
        uint8_t miso;
        uint8_t cs;
        uint8_t dc;
        uint8_t rst;
    } BoardPins;

    typedef struct
    {
        BoardType boardType;
        PicoType picoType;
        LibraryType libraryType;
        BoardPins pins;
        uint16_t width;
        uint16_t height;
        uint8_t rotation;
        const char *name;
        bool hasWiFi;
    } Board;

    static const Board VGMConfig = {
        .boardType = BOARD_TYPE_VGM,
        .picoType = PICO_TYPE_PICO,
        .libraryType = LIBRARY_TYPE_PICO_DVI,
        .pins = {
            .sck = 8,
            .mosi = 11,
            .miso = 12,
            .cs = 13,
            .dc = 14,
            .rst = 15},
        .width = 320,
        .height = 240,
        .rotation = 0,
        .name = "Video Game Module",
        .hasWiFi = false};

    static const Board PicoCalcConfigPico = {
        .boardType = BOARD_TYPE_PICO_CALC,
        .picoType = PICO_TYPE_PICO,
        .libraryType = LIBRARY_TYPE_TFT,
        .pins = {
            .sck = 10,
            .mosi = 11,
            .miso = 12,
            .cs = 13,
            .dc = 14,
            .rst = 15},
        .width = 320,
        .height = 320,
        .rotation = 0,
        .name = "PicoCalc - Pico",
        .hasWiFi = false};

    static const Board PicoCalcConfigPicoW = {
        .boardType = BOARD_TYPE_PICO_CALC,
        .picoType = PICO_TYPE_PICO_W,
        .libraryType = LIBRARY_TYPE_TFT,
        .pins = {
            .sck = 10,
            .mosi = 11,
            .miso = 12,
            .cs = 13,
            .dc = 14,
            .rst = 15},
        .width = 320,
        .height = 320,
        .rotation = 0,
        .name = "PicoCalc - Pico W",
        .hasWiFi = true};

    static const Board PicoCalcConfigPico2 = {
        .boardType = BOARD_TYPE_PICO_CALC,
        .picoType = PICO_TYPE_PICO_2,
        .libraryType = LIBRARY_TYPE_TFT,
        .pins = {
            .sck = 10,
            .mosi = 11,
            .miso = 12,
            .cs = 13,
            .dc = 14,
            .rst = 15},
        .width = 320,
        .height = 320,
        .rotation = 0,
        .name = "PicoCalc - Pico 2",
        .hasWiFi = false};

    static const Board PicoCalcConfigPico2W = {
        .boardType = BOARD_TYPE_PICO_CALC,
        .picoType = PICO_TYPE_PICO_2_W,
        .libraryType = LIBRARY_TYPE_TFT,
        .pins = {
            .sck = 10,
            .mosi = 11,
            .miso = 12,
            .cs = 13,
            .dc = 14,
            .rst = 15},
        .width = 320,
        .height = 320,
        .rotation = 0,
        .name = "PicoCalc - Pico 2 W",
        .hasWiFi = true};

    static const Board JBlankedPicoConfig = {
        .boardType = BOARD_TYPE_JBLANKED,
        .picoType = PICO_TYPE_PICO_W,
        .libraryType = LIBRARY_TYPE_TFT,
        .pins = {
            .sck = 6,
            .mosi = 7,
            .miso = 4,
            .cs = 5,
            .dc = 11,
            .rst = 10},
        .width = 320,
        .height = 240,
        .rotation = 3,
        .name = "JBlanked Pico",
        .hasWiFi = true};

};
