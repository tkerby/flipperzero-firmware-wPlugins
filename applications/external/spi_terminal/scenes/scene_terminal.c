#include "../flipper_spi_terminal.h"
#include "scenes.h"

#include <furi_hal_interrupt.h>

#include <stm32wbxx_ll_cortex.h>
#include <stm32wbxx_ll_dma.h>
#include <stm32wbxx_ll_spi.h>
#include <stm32wbxx_ll_utils.h>

#define spi_terminal_spi_bus_handle &furi_hal_spi_bus_handle_external
#define spi_terminal_spi_bus        furi_hal_spi_bus_handle_external.bus
#define spi_terminal_spi            SPI1

// Copy&Paste from furi_hal_spi.c
#define SPI_DMA            DMA2
#define SPI_DMA_RX_REQ     LL_DMAMUX_REQ_SPI1_RX
#define SPI_DMA_RX_CHANNEL LL_DMA_CHANNEL_6
#define SPI_DMA_RX_IRQ     FuriHalInterruptIdDma2Ch6
#define SPI_DMA_TX_REQ     LL_DMAMUX_REQ_SPI1_TX
#define SPI_DMA_TX_CHANNEL LL_DMA_CHANNEL_7
#define SPI_DMA_TX_IRQ     FuriHalInterruptIdDma2Ch7

typedef enum {
    FlipperSPITerminalAppTerminalEventReceived
} FlipperSPITerminalAppTerminalEvent;

void flipper_spi_terminal_scene_terminal_alloc(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("allocating terminal screen...");
    furi_check(app);

    // Main View
    app->terminalScreen.view = text_box_alloc();

    // Buffer for view text
    app->terminalScreen.output_string_buffer = furi_string_alloc();
    furi_string_reserve(app->terminalScreen.output_string_buffer, SPI_TERM_OUTPUT_BUFFER_SIZE);

    // Buffer for transfer from DMA to screen
    app->terminalScreen.rx_buffer_stream = furi_stream_buffer_alloc(128, 120);

    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneTerminal,
        text_box_get_view(app->terminalScreen.view));

    SPI_TERM_LOG_T("allocating terminal screen done!");
}

void flipper_spi_terminal_scene_terminal_free(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("Freeing terminal screen...");
    furi_check(app);

    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneTerminal);

    furi_string_free(app->terminalScreen.output_string_buffer);

    text_box_free(app->terminalScreen.view);
    SPI_TERM_LOG_T("Freeing terminal screen done!");
}

static void flipper_spi_terminal_scene_terminal_dma_rx_isr(void* context) {
    SPI_TERM_CONTEXT_TO_APP(context);

    if(!LL_DMA_IsEnabledIT_TC(SPI_DMA, SPI_DMA_RX_CHANNEL) &&
       !LL_DMA_IsEnabledIT_HT(SPI_DMA, SPI_DMA_RX_CHANNEL) &&
       !LL_DMA_IsEnabledIT_TE(SPI_DMA, SPI_DMA_RX_CHANNEL)) {
        return;
    }

    size_t halfDataSize = app->config.rx_dma_buffer_size / 2;
    uint8_t* startOfData = NULL;
    if(LL_DMA_IsActiveFlag_TC6(SPI_DMA)) { // Second half
        startOfData = app->terminalScreen.rx_dma_buffer + halfDataSize;
    } else if(LL_DMA_IsActiveFlag_HT6(SPI_DMA)) { // First half
        startOfData = app->terminalScreen.rx_dma_buffer;
    } else if(LL_DMA_IsActiveFlag_TE6(SPI_DMA)) { // Error
    }

    if(startOfData != NULL) {
        furi_stream_buffer_send(
            app->terminalScreen.rx_buffer_stream, startOfData, halfDataSize, 0);
    }

    LL_DMA_ClearFlag_HT6(SPI_DMA);
    LL_DMA_ClearFlag_TC6(SPI_DMA);
    LL_DMA_ClearFlag_TE6(SPI_DMA);
}

static void flipper_spi_terminal_scene_terminal_init_spi_dma(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("SPI/DMA Init...");
    furi_check(app);

    // this is mainly used for lockig of the SPI2 bus.
    SPI_TERM_LOG_T("Init SPI-Handle and lock");
    furi_hal_spi_bus_handle_init(spi_terminal_spi_bus_handle);
    furi_hal_spi_acquire(spi_terminal_spi_bus_handle);

    // Override the default SPI configuration for the R-Bus.
    // This will be reset as soon as furi_hal_spi_acquire is beeing called.
    SPI_TERM_LOG_T("Init SPI-Settings");
    LL_SPI_Disable(spi_terminal_spi);
    LL_SPI_Init(spi_terminal_spi, &app->config.spi);
    LL_SPI_Enable(spi_terminal_spi);

    SPI_TERM_LOG_T("Init DMA");
    LL_DMA_InitTypeDef dma_config = {
        .PeriphOrM2MSrcAddress = (uint32_t)&spi_terminal_spi->DR,
        .MemoryOrM2MDstAddress = (uint32_t)app->terminalScreen.rx_dma_buffer,
        .Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY,
        .Mode = LL_DMA_MODE_CIRCULAR,
        .PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT,
        .MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT,
        .PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE,
        .MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE,
        .NbData = app->config.rx_dma_buffer_size,
        .PeriphRequest = SPI_DMA_RX_REQ,
        .Priority = LL_DMA_PRIORITY_MEDIUM,
    };
    LL_DMA_Init(SPI_DMA, SPI_DMA_RX_CHANNEL, &dma_config);

    // Clear existing Transfer complete Flag
    LL_DMA_ClearFlag_TC6(SPI_DMA);

    SPI_TERM_LOG_T("Enabling DMA...");

    // Settings Interrupt handler for DMA Transfer
    furi_hal_interrupt_set_isr(
        SPI_DMA_RX_IRQ, flipper_spi_terminal_scene_terminal_dma_rx_isr, app);

    // Enable SPI to DMA
    LL_SPI_EnableDMAReq_RX(spi_terminal_spi);

    // Enable DMA Error handle
    LL_DMA_EnableIT_TE(SPI_DMA, SPI_DMA_RX_CHANNEL);
    // Enable half Transfer Complete
    LL_DMA_EnableIT_HT(SPI_DMA, SPI_DMA_RX_CHANNEL);
    // Enable Transfer Complete
    LL_DMA_EnableIT_TC(SPI_DMA, SPI_DMA_RX_CHANNEL);

    // Start DMA
    LL_DMA_EnableChannel(SPI_DMA, SPI_DMA_RX_CHANNEL);
}

static void flipper_spi_terminal_scene_terminal_deinit_spi_dma(FlipperSPITerminalApp* app) {
    furi_check(app);

    SPI_TERM_LOG_T("Disabling DMA");
    // Stop DMA
    LL_DMA_DisableChannel(SPI_DMA, SPI_DMA_RX_CHANNEL);

    // Disable DMA Error handle
    LL_DMA_DisableIT_TE(SPI_DMA, SPI_DMA_RX_CHANNEL);
    // Disable half Transfer Complete
    LL_DMA_DisableIT_HT(SPI_DMA, SPI_DMA_RX_CHANNEL);
    // Disable Transfer Complete
    LL_DMA_DisableIT_TC(SPI_DMA, SPI_DMA_RX_CHANNEL);

    // Clear interrupt
    furi_hal_interrupt_set_isr(SPI_DMA_RX_IRQ, NULL, NULL);

    // Disable SPI to DMA
    LL_SPI_DisableDMAReq_RX(spi_terminal_spi);

    SPI_TERM_LOG_T("Deinit DMA");
    LL_DMA_DeInit(SPI_DMA, SPI_DMA_RX_CHANNEL);

    SPI_TERM_LOG_T("Unlocking SPI");
    furi_hal_spi_release(spi_terminal_spi_bus_handle);
    furi_hal_spi_bus_handle_deinit(spi_terminal_spi_bus_handle);
}

void flipper_spi_terminal_scene_terminal_on_enter(void* context) {
    SPI_TERM_LOG_T("Enter Terminal");
    SPI_TERM_CONTEXT_TO_APP(context);

    furi_string_reset(app->terminalScreen.output_string_buffer);
    text_box_set_text(
        app->terminalScreen.view, furi_string_get_cstr(app->terminalScreen.output_string_buffer));

    furi_stream_buffer_reset(app->terminalScreen.rx_buffer_stream);

    // Minimum of 2 bytes for rx buffer
    furi_check(app->config.rx_dma_buffer_size >= 2);
    furi_check((app->config.rx_dma_buffer_size % 2) == 0);
    app->terminalScreen.rx_dma_buffer = malloc(app->config.rx_dma_buffer_size);

    flipper_spi_terminal_scene_terminal_init_spi_dma(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneTerminal);
}

bool flipper_spi_terminal_scene_terminal_on_event(void* context, SceneManagerEvent event) {
    SPI_TERM_CONTEXT_TO_APP(context);

    if(event.type == SceneManagerEventTypeTick) {
        bool addedAny = false;
        for(size_t i = 0; i < 3; i++) {
            uint8_t buffer[32];
            size_t count = furi_stream_buffer_receive(
                app->terminalScreen.rx_buffer_stream, buffer, sizeof(buffer) - 1, 0);
            if(count == 0)
                break;
            else
                addedAny = true;

            buffer[count] = 0;

            furi_string_cat_printf(app->terminalScreen.output_string_buffer, "%s", buffer);
        }

        if(addedAny) {
            text_box_set_text(
                app->terminalScreen.view,
                furi_string_get_cstr(app->terminalScreen.output_string_buffer));

            text_box_set_focus(app->terminalScreen.view, TextBoxFocusEnd);
        }
    }

    return false;
}

void flipper_spi_terminal_scene_terminal_on_exit(void* context) {
    SPI_TERM_LOG_T("Exit Terminal");
    SPI_TERM_CONTEXT_TO_APP(context);

    flipper_spi_terminal_scene_terminal_deinit_spi_dma(app);

    free(app->terminalScreen.rx_dma_buffer);
    app->terminalScreen.rx_dma_buffer = NULL;
}
