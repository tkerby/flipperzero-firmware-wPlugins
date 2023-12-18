
#include "SPI.hpp"
#include "furi_hal_spi.h"
#define SPI_TIMEOUT 1000

SPIClass SPI;

uint8_t SPIClass::transfer(uint8_t tx_val) {
    uint8_t ret;
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, &tx_val, 1, SPI_TIMEOUT);
    furi_hal_spi_bus_rx(&furi_hal_spi_bus_handle_external, &ret, 1, SPI_TIMEOUT);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    return ret;
}

void SPIClass::beginTransaction(SPISettings __attribute__((unused)) settings) {
    return;
}
void SPIClass::endTransaction(void) {
    return;
}

void SPIClass::begin(void) {
    return;
}