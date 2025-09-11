#pragma once

#include <stdint.h>

class VEML7700 {
public:
    /**
     * @brief Konstruktor dla klasy VEML7700.
     * @param i2c_address_7bit 7-bitowy adres I2C czujnika.
     */
    VEML7700(uint8_t i2c_address_7bit);

    /**
     * @brief Inicjalizuje czujnik VEML7700.
     * @return Prawda, jeśli inicjalizacja zakończyła się sukcesem.
     */
    bool init();

    /**
     * @brief Odczytuje wartość luksów z czujnika.
     * @param lux_value Wskaźnik, pod który zostanie zapisana odczytana wartość.
     * @return Prawda, jeśli odczyt zakończył się sukcesem.
     */
    bool readLux(float& lux_value);

private:
    uint8_t _i2c_addr_8bit;
};
