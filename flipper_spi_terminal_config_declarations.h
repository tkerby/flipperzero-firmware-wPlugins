#ifndef SPI_CONFIG_ADDED_INCLUDES
#include <furi_hal_spi_types.h>
#include "toolbox/value_index_ex.h"
#define SPI_CONFIG_ADDED_INCLUDES
#endif

#ifndef UNWRAP_ARGS
// Removes brackets from a list of values
#define UNWRAP_ARGS(...) __VA_ARGS__
#endif

#ifndef ADD_CONFIG_ENTRY
// This is just a dummy function to enable some fancy syntax highlighting and autocomplete.
// It will only be used at edit time.
#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    void(dummy_config_##name##_preview_func)() {                                                    \
        printf(label);                                                                              \
        printf(helpText);                                                                           \
        const type def = defaultValue;                                                              \
        const type vals[valuesCount] = {UNWRAP_ARGS values};                                        \
        size_t index = (valueIndexFunc)(def, vals, valuesCount);                                    \
        UNUSED(index);                                                                              \
        const char* const strs[valuesCount] = {UNWRAP_ARGS strings};                                \
        UNUSED(strs);                                                                               \
    }
#endif

ADD_CONFIG_ENTRY(
    "Display mode",
    "Changes, how the received data is displayed.\n"
    "Auto: Use ASCII, C escape sequence or Hex\n"
    "Text: Use ASCII for everything (Non printable chars are replaced by a space ' ')\n"
    "Hex: Use Hex for everything\n"
    "Binary: Binary representation\n\n"
    "Default: Auto",
    display_mode,
    TerminalDisplayMode,
    TerminalDisplayModeAuto,
    value_index_display_mode,
    display_mode,
    4,
    (TerminalDisplayModeAuto,
     TerminalDisplayModeText,
     TerminalDisplayModeHex,
     TerminalDisplayModeBinary),
    ("Auto", "Text", "Hex", "Binary"))

ADD_CONFIG_ENTRY(
    "DMA RX Buffer size",
    "Sets the buffer size for a SPI read request. A higher value results in a less frequent update. A higher value will result in a faster data rate.\n\n"
    "SPI Terminal utilizes the inbuild DMA controller for a event/interrupt based receive and transmit system. This removes the overhead of a polling based system.\n\n"
    "Default: 1",
    rx_dma_buffer_size,
    size_t,
    1,
    value_index_size_t,
    rx_dma_buffer_size,
    9,
    (1, 2, 4, 8, 16, 32, 64, 128, 256),
    ("1", "2", "4", "8", "16", "32", "64", "128", "256"))

ADD_CONFIG_ENTRY(
    "Mode",
    "Master: Flipper is controlling the SPI clock\n"
    "Slave: A external device is controlling the SPI clock\n\n"
    "Default: Slave",
    spi_mode,
    uint32_t,
    LL_SPI_MODE_SLAVE,
    value_index_uint32,
    spi.Mode,
    2,
    (LL_SPI_MODE_MASTER, LL_SPI_MODE_SLAVE),
    ("Master", "Slave"))

ADD_CONFIG_ENTRY(
    "Direction",
    "Full Duplex: Data is transmitted in both directions (Rx and Tx)\n"
    "Half Duplex RX: Only send data\n"
    "Half Duplex TX: Only receive data\n\n"
    "Default: Full Duplex",
    spi_direction,
    uint32_t,
    LL_SPI_FULL_DUPLEX,
    value_index_uint32,
    spi.TransferDirection,
    3,
    (LL_SPI_FULL_DUPLEX, LL_SPI_HALF_DUPLEX_RX, LL_SPI_HALF_DUPLEX_TX),
    ("Full Duplex", "Half Duplex RX", "Half Duplex TX"))

ADD_CONFIG_ENTRY(
    "Data Width",
    "Sets the data width of a single transfer.\n\n"
    "Default: 8 bit",
    spi_data_width,
    uint32_t,
    LL_SPI_DATAWIDTH_8BIT,
    value_index_uint32,
    spi.DataWidth,
    13,
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
    "Low: Clock line is 0 (low, off) on idle\n"
    "High: Clock line is 1 (high, on) on idle\n\n"
    "Default: High",
    spi_clock_polarity,
    uint32_t,
    LL_SPI_POLARITY_LOW,
    value_index_uint32,
    spi.ClockPolarity,
    2,
    (LL_SPI_POLARITY_LOW, LL_SPI_POLARITY_HIGH),
    ("Low", "Heigh"))

ADD_CONFIG_ENTRY(
    "Clock Phase",
    "Defines, on which clock edge the data is sampled.\n"
    "1. Edge: Trigger on the first clock polarity change (CP: High, CLK: ...1 1010 1010 1... => Trigger on the transition from 1 to 0)\n"
    "2. Edge: Trigger on the second clock polarity change (CP: High, CLK: ...1 1010 1010 1... => Trigger on the transition from 0 to 1)\n\n"
    "Default: !-Edge",
    spi_clock_phase,
    uint32_t,
    LL_SPI_PHASE_1EDGE,
    value_index_uint32,
    spi.ClockPhase,
    2,
    (LL_SPI_PHASE_1EDGE, LL_SPI_PHASE_2EDGE),
    ("1-Edge", "2-Edge"))

ADD_CONFIG_ENTRY(
    "NSS",
    "Configures the chip select behavior.\n"
    "Soft: NSS managed internally. NSS pin not used and free. Always send/receive.\n"
    "Input: NSS pin used in Input. Only used in Master mode.\n"
    "Hard: NSS pin used in Output. Only used in Slave mode as chip select.\n\n"
    "Default: Soft",
    spi_nss,
    uint32_t,
    LL_SPI_NSS_SOFT,
    value_index_uint32,
    spi.NSS,
    3,
    (LL_SPI_NSS_SOFT, LL_SPI_NSS_HARD_INPUT, LL_SPI_NSS_HARD_OUTPUT),
    ("Soft", "Input", "Hard"))

ADD_CONFIG_ENTRY(
    "Baudrate prescaler",
    "Sets, by how many cycles, the SPI clock is divided. Flippers SPI clock is running at 32MHz. A prescaler of Div 2 will result int a baudrate of around 16 Mbit.\n"
    "/ 2 = 16 Mbit\n"
    "/ 4 = 8 Mbit\n"
    "/ 8 = 4 Mbit\n"
    "/ 16 = 2 Mbit\n"
    "/ 32 = 1 Mbit\n"
    "/ 64 = 500 Kbit\n"
    "/ 128 = 250 Kbit\n"
    "/ 256 = 125 Kbit\n\n"
    "Default: Div 32",
    spi_baud_rate,
    uint32_t,
    LL_SPI_BAUDRATEPRESCALER_DIV32,
    value_index_uint32,
    spi.BaudRate,
    8,
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
    "Sets, in which order bytes are received.\n\n"
    "Default: MSB",
    spi_bit_order,
    uint32_t,
    LL_SPI_MSB_FIRST,
    value_index_uint32,
    spi.BitOrder,
    2,
    (LL_SPI_LSB_FIRST, LL_SPI_MSB_FIRST),
    ("LSB", "MSB"))

ADD_CONFIG_ENTRY(
    "CRC",
    "Enables or disables the CRC calculation.\n\n"
    "Default: Disabled",
    spi_crc_calculation,
    uint32_t,
    LL_SPI_CRCCALCULATION_DISABLE,
    value_index_uint32,
    spi.CRCCalculation,
    2,
    (LL_SPI_CRCCALCULATION_DISABLE, LL_SPI_CRCCALCULATION_ENABLE),
    ("Disabled", "Enabled"))

ADD_CONFIG_ENTRY(
    "CRC Poly",
    "Sets the CRC Polynomial value.\n\n"
    "Default: 7",
    spi_crc_poly,
    uint32_t,
    7,
    value_index_uint32,
    spi.CRCPoly,
    10,
    (2, 3, 4, 5, 6, 7, 8, 9, 10, 11),
    ("2", "3", "4", "5", "6", "7", "8", "9", "10", "11"))
