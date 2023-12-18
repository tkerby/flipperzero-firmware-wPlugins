/**
 * @file: SPI.h
 * @details : bridge for porting Longan_CANFD to flipper without modifying code. A simple pull to the repo shall be OK.
*/
#pragma once
#include <stdint.h>

typedef enum { SPI_MODE_0, SPI_MODE_1, SPI_MODE_2, SPI_MODE_3 } e_spi_mode_t;

typedef enum { MSB_FIRST, LSB_FIRST } e_spi_endiannes_t;

typedef uint8_t byte;
#define pinMode(X, Y)
#define digitalWrite(X, Y)

class SPISettings {
    uint32_t freqHz;
    e_spi_endiannes_t endianness;
    e_spi_mode_t mode;
    uint32_t BitOrder;
};

class SPIClass {
public:
    uint8_t transfer(uint8_t tx_val);
    void beginTransaction(SPISettings settings);
    void endTransaction(void);
    void begin(void);
};
extern SPIClass SPI;