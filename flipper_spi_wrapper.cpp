
#include "SPI.hpp"
#include "furi_hal_spi.h"
#define SPI_TIMEOUT 1000

SPIClass SPI;
static uint8_t tx(uint8_t tx_val);
uint8_t (*transfer_spiTransmitBuffer[98])(uint8_t tx_val) = {
    tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx,
    tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx,
    tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx,
    tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx, tx,
    tx,tx,tx,tx,tx,tx,tx,tx,tx,tx
};

static uint8_t tx(uint8_t tx_val) {
    uint8_t ret = 0;
    furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, &tx_val, 1, SPI_TIMEOUT);
    return ret;
}

uint8_t rx(uint8_t tx_val) {
    uint8_t ret;
    (void)tx_val;
    furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_external, NULL, &ret, 1, SPI_TIMEOUT);
    return ret;
}

void SPIClass::beginTransaction(SPISettings __attribute__((unused)) settings) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    return;
}
void SPIClass::endTransaction(void) {
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    return;
}

void SPIClass::begin(void) {
    return;
}

SPISettings::SPISettings(
    uint32_t __attribute__((unused)) freqHz,
    e_spi_endiannes_t __attribute__((unused)) endianness,
    e_spi_mode_t __attribute__((unused)) mode) {
}