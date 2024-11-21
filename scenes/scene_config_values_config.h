#ifndef ADD_CONFIG_ENTRY
#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings)
#endif

#include <furi_hal_spi_types.h>

ADD_CONFIG_ENTRY(
    "Mode",
    mode,
    uint32_t,
    value_index_uint32,
    Mode,
    (LL_SPI_MODE_MASTER, LL_SPI_MODE_SLAVE),
    ("Master", "Slave"))

ADD_CONFIG_ENTRY(
    "Direction",
    direction,
    uint32_t,
    value_index_uint32,
    TransferDirection,
    (LL_SPI_FULL_DUPLEX, LL_SPI_SIMPLEX_RX, LL_SPI_HALF_DUPLEX_RX, LL_SPI_HALF_DUPLEX_TX),
    ("Full Duplex", "Simplex RX", "Half Duplex RX", "Half Duplex TX"))

ADD_CONFIG_ENTRY(
    "Data Width",
    data_width,
    uint32_t,
    value_index_uint32,
    DataWidth,
    (LL_SPI_DATAWIDTH_4BIT,
     LL_SPI_DATAWIDTH_5BIT,
     LL_SPI_DATAWIDTH_6BIT,
     LL_SPI_DATAWIDTH_7BIT,
     LL_SPI_DATAWIDTH_8BIT,
     LL_SPI_DATAWIDTH_9BIT,
     LL_SPI_DATAWIDTH_10BIT,
     LL_SPI_DATAWIDTH_11BIT,
     LL_SPI_DATAWIDTH_12BIT,
     LL_SPI_DATAWIDTH_13BIT,
     LL_SPI_DATAWIDTH_14BIT,
     LL_SPI_DATAWIDTH_15BIT,
     LL_SPI_DATAWIDTH_16BIT),
    ("4 bit",
     "5 bit",
     "6 bit",
     "7 bit",
     "8 bit",
     "9 bit",
     "10 bit",
     "11 bit",
     "12 bit",
     "13 bit",
     "14 bit",
     "15 bit",
     "16 bit"))

ADD_CONFIG_ENTRY(
    "Clock polarity",
    clock_polarity,
    uint32_t,
    value_index_uint32,
    ClockPolarity,
    (LL_SPI_POLARITY_LOW, LL_SPI_POLARITY_HIGH),
    ("Low", "Heigh"))

ADD_CONFIG_ENTRY(
    "Clock Phase",
    clock_phase,
    uint32_t,
    value_index_uint32,
    ClockPhase,
    (LL_SPI_PHASE_1EDGE, LL_SPI_PHASE_2EDGE),
    ("1-Edge", "2-Edge"))

ADD_CONFIG_ENTRY(
    "NSS",
    nss,
    uint32_t,
    value_index_uint32,
    NSS,
    (LL_SPI_NSS_SOFT, LL_SPI_NSS_HARD_INPUT, LL_SPI_NSS_HARD_OUTPUT),
    ("soft", "input", "hard"))

ADD_CONFIG_ENTRY(
    "Baudrate",
    baud_rate,
    uint32_t,
    value_index_uint32,
    BaudRate,
    (LL_SPI_BAUDRATEPRESCALER_DIV2,
     LL_SPI_BAUDRATEPRESCALER_DIV4,
     LL_SPI_BAUDRATEPRESCALER_DIV8,
     LL_SPI_BAUDRATEPRESCALER_DIV16,
     LL_SPI_BAUDRATEPRESCALER_DIV32,
     LL_SPI_BAUDRATEPRESCALER_DIV64,
     LL_SPI_BAUDRATEPRESCALER_DIV128,
     LL_SPI_BAUDRATEPRESCALER_DIV256),
    ("Div 2", "Div 4", "Div 8", "Div 16", "Div 32", "Div 64", "Div 128", "Div 256"))

ADD_CONFIG_ENTRY(
    "Bit-order",
    bit_order,
    uint32_t,
    value_index_uint32,
    BitOrder,
    (LL_SPI_LSB_FIRST, LL_SPI_MSB_FIRST),
    ("LSB", "MSB"))

ADD_CONFIG_ENTRY(
    "CRC",
    crc_calculation,
    uint32_t,
    value_index_uint32,
    CRCCalculation,
    (LL_SPI_CRCCALCULATION_DISABLE, LL_SPI_CRCCALCULATION_ENABLE),
    ("Disabled", "Enabled"))

ADD_CONFIG_ENTRY(
    "CRC Poly",
    crc_poly,
    uint32_t,
    value_index_uint32,
    CRCPoly,
    (2, 3, 4, 5, 6, 7, 8, 9, 10, 11),
    ("2", "3", "4", "5", "6", "7", "8", "9", "10", "11"))
