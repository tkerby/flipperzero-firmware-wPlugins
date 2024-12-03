#ifndef ADD_CONFIG_ENTRY
#define ADD_CONFIG_ENTRY( \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, values, strings)
#endif

#include <furi_hal_spi_types.h>

ADD_CONFIG_ENTRY(
    "Display mode",
    "Changes, how the received data is displayed.\n"
    "Auto: Use ASCII or C escape sequence\n"
    "Hex: Use Hex for everything\n"
    "Binary: Binary representation",
    display_mode,
    FlipperSPITerminalAppDisplayMode,
    FlipperSPITerminalAppDisplayModeAuto,
    value_index_display_mode,
    display_mode,
    (FlipperSPITerminalAppDisplayModeAuto,
     FlipperSPITerminalAppDisplayModeHex,
     FlipperSPITerminalAppDisplayModeBinary),
    ("Auto", "Hex", "Binary"))

ADD_CONFIG_ENTRY(
    "DMA RX Buffer size",
    "Sets the buffer size for a SPI read request. A higher value results in a less frequent update. A higher value will result in a faster data rate.\n\n"
    "SPI Terminal utilizes the inbuild DMA controller for a event/interrupt based receive and transmit system. This removes the overhead of a polling based system.",
    rx_dma_buffer_size,
    size_t,
    1,
    value_index_size_t,
    rx_dma_buffer_size,
    (1, 2, 4, 8, 16, 32, 64, 128, 256),
    ("1", "2", "4", "8", "16", "32", "64", "128", "256"))

ADD_CONFIG_ENTRY(
    "Mode",
    "Master: Flipper is controlling the SPI clock\n"
    "Slave: A external device is controlling the SPI clock",
    spi_mode,
    uint32_t,
    LL_SPI_MODE_SLAVE,
    value_index_uint32,
    spi.Mode,
    (LL_SPI_MODE_MASTER, LL_SPI_MODE_SLAVE),
    ("Master", "Slave"))

ADD_CONFIG_ENTRY(
    "Direction",
    "Full Duplex: Data is transmitted in both directions (Rx and Tx)\n"
    "Half Duplex RX: Only send data\n"
    "Half Duplex TX: Only receive data",
    spi_direction,
    uint32_t,
    LL_SPI_FULL_DUPLEX,
    value_index_uint32,
    spi.TransferDirection,
    (LL_SPI_FULL_DUPLEX, LL_SPI_HALF_DUPLEX_RX, LL_SPI_HALF_DUPLEX_TX),
    ("Full Duplex", "Half Duplex RX", "Half Duplex TX"))

ADD_CONFIG_ENTRY(
    "Data Width",
    "Sets the data width of a single transfer.",
    spi_data_width,
    uint32_t,
    LL_SPI_DATAWIDTH_8BIT,
    value_index_uint32,
    spi.DataWidth,
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
    "High: Clock line is 1 (high, on) on idle",
    spi_clock_polarity,
    uint32_t,
    LL_SPI_POLARITY_LOW,
    value_index_uint32,
    spi.ClockPolarity,
    (LL_SPI_POLARITY_LOW, LL_SPI_POLARITY_HIGH),
    ("Low", "Heigh"))

ADD_CONFIG_ENTRY(
    "Clock Phase",
    "Defines, on which clock edge the data is sampled.\n"
    "1. Edge: Trigger on the first clock polarity change (CP: High, CLK: ...1 1010 1010 1... => Trigger on the transition from 1 to 0)\n"
    "2. Edge: Trigger on the second clock polarity change (CP: High, CLK: ...1 1010 1010 1... => Trigger on the transition from 0 to 1)",
    spi_clock_phase,
    uint32_t,
    LL_SPI_PHASE_1EDGE,
    value_index_uint32,
    spi.ClockPhase,
    (LL_SPI_PHASE_1EDGE, LL_SPI_PHASE_2EDGE),
    ("1-Edge", "2-Edge"))

ADD_CONFIG_ENTRY(
    "NSS",
    "Configures the chip select behavior.\n"
    "Soft: NSS managed internally. NSS pin not used and free. Always send/receive.\n"
    "Input: NSS pin used in Input. Only used in Master mode.\n"
    "Hard: NSS pin used in Output. Only used in Slave mode as chip select.",
    spi_nss,
    uint32_t,
    LL_SPI_NSS_SOFT,
    value_index_uint32,
    spi.NSS,
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
    "/ 256 = 125 Kbit",
    spi_baud_rate,
    uint32_t,
    LL_SPI_BAUDRATEPRESCALER_DIV32,
    value_index_uint32,
    spi.BaudRate,
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
    "Sets, in which order bytes are received.",
    spi_bit_order,
    uint32_t,
    LL_SPI_MSB_FIRST,
    value_index_uint32,
    spi.BitOrder,
    (LL_SPI_LSB_FIRST, LL_SPI_MSB_FIRST),
    ("LSB", "MSB"))

ADD_CONFIG_ENTRY(
    "CRC",
    "Enables or disables the CRC calculation.",
    spi_crc_calculation,
    uint32_t,
    LL_SPI_CRCCALCULATION_DISABLE,
    value_index_uint32,
    spi.CRCCalculation,
    (LL_SPI_CRCCALCULATION_DISABLE, LL_SPI_CRCCALCULATION_ENABLE),
    ("Disabled", "Enabled"))

ADD_CONFIG_ENTRY(
    "CRC Poly",
    "Sets the CRC Polynomial value.",
    spi_crc_poly,
    uint32_t,
    7,
    value_index_uint32,
    spi.CRCPoly,
    (2, 3, 4, 5, 6, 7, 8, 9, 10, 11),
    ("2", "3", "4", "5", "6", "7", "8", "9", "10", "11"))
