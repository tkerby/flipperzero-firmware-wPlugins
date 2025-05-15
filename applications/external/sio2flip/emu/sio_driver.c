/*
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <furi_hal_resources.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_gpio.h>

#include "app/app_common.h"

#include "sio_driver.h"

#define MAX_SIO_DEVICES 4

#define SIO_DEFAULT_BAUDRATE 19200

// PC1 [pin 15] <- SIO COMMAND
#define GPIO_EXT_COMMAND  &gpio_ext_pc0
// PC0 [pin 16] <- SIO MOTORCTL
#define GPIO_EXT_MOTORCTL &gpio_ext_pc1

// PB6/USART1.TX [pin 13] -> SIO DIN
// PB7/USART1.RX [pin 14] -> SIO DOUT
#define GPIO_EXT_UART FuriHalSerialIdUsart

#define WORKER_EVT_STOP    (1 << 0)
#define WORKER_EVT_RECEIVE (1 << 1)
#define WORKER_EVT_ERROR   (1 << 3)

#define WORKER_EVT_ALL (WORKER_EVT_STOP | WORKER_EVT_RECEIVE | WORKER_EVT_ERROR)

typedef struct {
    SIODevice device;
    SIOCallback command_callback;
    SIOCallback data_callback;
    void* callback_context;
} SIOHandler;

struct SIODriver {
    bool command_phase;
    size_t rx_count;
    size_t rx_expected;

    // Currently used baudrate
    uint32_t baudrate;
    // Baudrate for the next command
    uint32_t req_baudrate;
    // Alternative baudrate (used for baudrate swapping)
    uint32_t alt_baudrate;
    // Errors counter used for baudrate swapping
    uint32_t error_count;

    FuriHalSerialHandle* uart;
    FuriThread* rx_thread;
    SIORequest request;

    SIOHandler handler[MAX_SIO_DEVICES];
};

static uint8_t calc_chksum(const uint8_t* data, size_t size) {
    uint32_t chksum = 0;
    for(size_t i = 0; i < size; i++) {
        chksum += data[i];
        if(chksum >= 0x100) {
            chksum = (chksum & 0xFF) + 1;
        }
    }
    return chksum;
}

static void sio_set_baudrate(SIODriver* sio, uint32_t baudrate) {
    furi_check(sio != NULL);

    if(baudrate != sio->baudrate) {
        sio->baudrate = baudrate;
        furi_hal_serial_tx_wait_complete(sio->uart);
        furi_hal_serial_set_br(sio->uart, sio->baudrate);
    }
}

static void sio_swap_baudrate(SIODriver* sio) {
    furi_check(sio != NULL);

    if(sio->baudrate == SIO_DEFAULT_BAUDRATE) {
        sio->req_baudrate = sio->alt_baudrate;
    } else {
        sio->req_baudrate = SIO_DEFAULT_BAUDRATE;
    }
}

static void sio_driver_send_status(SIODriver* sio, uint8_t status) {
    if(status != SIO_NORESP) {
        FURI_LOG_I(TAG, "SIO: sending %02X status", status);
        furi_delay_ms(1);
        furi_hal_serial_tx(sio->uart, &status, sizeof(status));
    }
}

static void sio_driver_send_data(SIODriver* sio, const uint8_t* data, size_t size) {
    if(size > 0) {
        FURI_LOG_I(TAG, "SIO: sending %d bytes of data", sio->request.tx_size);
        furi_delay_ms(1);
        uint8_t chksum = calc_chksum(data, size);
        furi_hal_serial_tx(sio->uart, data, size);
        furi_hal_serial_tx(sio->uart, &chksum, 1);
    }
}

static SIOStatus sio_invoke_command_callback(SIODriver* sio, SIORequest* request) {
    for(int i = 0; i < MAX_SIO_DEVICES; i++) {
        SIOHandler* handler = &sio->handler[i];
        if(handler->device == request->device) {
            SIOCallback callback = handler->command_callback;
            void* context = handler->callback_context;
            if(callback) {
                return callback(context, request);
            }
        }
    }

    return SIO_NORESP;
}

static SIOStatus sio_invoke_data_callback(SIODriver* sio, SIORequest* request) {
    for(int i = 0; i < MAX_SIO_DEVICES; i++) {
        SIOHandler* handler = &sio->handler[i];
        if(handler->device == request->device) {
            SIOCallback callback = handler->data_callback;
            void* context = handler->callback_context;
            if(callback) {
                return callback(context, request);
            }
        }
    }

    return SIO_NORESP;
}

static void sio_driver_command_pin_callback(void* context) {
    SIODriver* sio = (SIODriver*)context;

    bool command_low = !furi_hal_gpio_read(GPIO_EXT_COMMAND);

    if(command_low) {
        if(!sio->command_phase) {
            // Start of command
            sio->command_phase = true;
            sio->rx_count = 0;
            sio->rx_expected = 4; // Device, Command, Aux1, Aux2
        } else {
            // Command pin went log again during command phase
            furi_thread_flags_set(furi_thread_get_id(sio->rx_thread), WORKER_EVT_ERROR);
        }

        if(sio->baudrate != sio->req_baudrate) {
            // Restore the requested baudrate
            sio_set_baudrate(sio, sio->req_baudrate);
        }
    }
}

void sio_driver_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    SIODriver* sio = (SIODriver*)context;

    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            uint8_t rx_byte = furi_hal_serial_async_rx(handle);

            if(sio->rx_count < sio->rx_expected) {
                // Store received byte
                sio->request.rx_data[sio->rx_count++] = rx_byte;
            } else if(sio->rx_expected > 0 && sio->rx_count == sio->rx_expected) {
                // Checksum received
                sio->rx_expected = 0;

                if(rx_byte == calc_chksum(sio->request.rx_data, sio->rx_count)) {
                    // Checksum OK

                    if(sio->command_phase) {
                        sio->request.rx_size = 0;
                        sio->request.tx_size = 0;
                        sio->request.device = sio->request.rx_data[0];
                        sio->request.command = sio->request.rx_data[1];
                        sio->request.aux1 = sio->request.rx_data[2];
                        sio->request.aux2 = sio->request.rx_data[3];
                    } else {
                        sio->request.rx_size = sio->rx_count;
                    }
                    furi_thread_flags_set(furi_thread_get_id(sio->rx_thread), WORKER_EVT_RECEIVE);

                } else {
                    // Checksum error
                    furi_thread_flags_set(furi_thread_get_id(sio->rx_thread), WORKER_EVT_ERROR);
                }
            }
        }
    }
}

static int32_t sio_driver_rx_worker(void* context) {
    SIODriver* sio = (SIODriver*)context;

    for(;;) {
        uint32_t events = furi_thread_flags_wait(WORKER_EVT_ALL, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WORKER_EVT_STOP) {
            // Worker thread is stopping
            break;
        } else if(events & WORKER_EVT_ERROR) {
            FURI_LOG_E(
                TAG,
                "SIO: frame error (%d bytes received - %02X %02X %02X %02X...)",
                sio->rx_count,
                sio->request.rx_data[0],
                sio->request.rx_data[1],
                sio->request.rx_data[2],
                sio->request.rx_data[3]);

            if(sio->command_phase) {
                sio->error_count++;
                if(sio->error_count >= 2) {
                    sio->error_count = 0;
                    // Swap baudrate after 2 consecutive errors
                    // (US Doubler high-speed mode)
                    sio_swap_baudrate(sio);
                }
            }

            sio->command_phase = false;
        } else if(events & WORKER_EVT_RECEIVE) {
            sio->error_count = 0;

            if(sio->command_phase) {
                FURI_LOG_I(
                    TAG,
                    "SIO: command frame received: %02X %02X %02X %02X",
                    sio->request.device,
                    sio->request.command,
                    sio->request.aux1,
                    sio->request.aux2);
            } else {
                FURI_LOG_I(TAG, "SIO: data frame received (%d bytes)", sio->rx_count);
            }

            if(sio->command_phase) {
                // Process the command frame
                sio->request.baudrate = sio->baudrate;
                uint8_t status = sio_invoke_command_callback(sio, &sio->request);

                __disable_irq();
                sio->rx_count = 0;
                sio->rx_expected = sio->request.rx_size;
                sio->command_phase = false;
                __enable_irq();

                // ACK or NAK the command frame
                sio_driver_send_status(sio, status);

                if(status == SIO_ACK) {
                    if(sio->request.baudrate != sio->baudrate) {
                        // Temporary baudrate for the data frame
                        // (XF551 high-speed mode)
                        sio->alt_baudrate = SIO_DEFAULT_BAUDRATE;
                        sio->req_baudrate = SIO_DEFAULT_BAUDRATE;
                        sio_set_baudrate(sio, sio->request.baudrate);
                        sio->request.baudrate = SIO_DEFAULT_BAUDRATE;
                    }

                    if(sio->rx_expected == 0) {
                        // Execute the command without receiving data
                        uint8_t status = sio_invoke_data_callback(sio, &sio->request);

                        sio_driver_send_status(sio, status);
                        sio_driver_send_data(sio, sio->request.tx_data, sio->request.tx_size);

                        if(sio->request.baudrate != sio->req_baudrate) {
                            // Permanent baudrate change for all subsequent commands
                            // (US Doubler high-speed mode)
                            sio->alt_baudrate = sio->request.baudrate;
                            sio->req_baudrate = sio->request.baudrate;
                            FURI_LOG_D(TAG, "SIO: baudrate changed to %ld", sio->req_baudrate);
                        }
                    }
                }
            } else {
                // ACK data frame
                sio_driver_send_status(sio, SIO_ACK);

                // Execute the command with received data
                uint8_t status = sio_invoke_data_callback(sio, &sio->request);

                sio_driver_send_status(sio, status);
                sio_driver_send_data(sio, sio->request.tx_data, sio->request.tx_size);
            }
        }
    }
    return 0;
}

SIODriver* sio_driver_alloc() {
    SIODriver* sio = malloc(sizeof(SIODriver));
    memset(sio, 0, sizeof(SIODriver));

    furi_hal_gpio_init(GPIO_EXT_COMMAND, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(GPIO_EXT_COMMAND, GpioModeInterruptRiseFall, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(GPIO_EXT_MOTORCTL, GpioModeInput, GpioPullNo, GpioSpeedLow);

    sio->baudrate = SIO_DEFAULT_BAUDRATE;
    sio->req_baudrate = SIO_DEFAULT_BAUDRATE;
    sio->alt_baudrate = SIO_DEFAULT_BAUDRATE;
    sio->uart = furi_hal_serial_control_acquire(GPIO_EXT_UART);

    if(!sio->uart) {
        FURI_LOG_E(TAG, "Failed to acquire UART");
        goto cleanup;
    }

    furi_hal_serial_init(sio->uart, sio->baudrate);

    sio->rx_thread = furi_thread_alloc();
    if(!sio->rx_thread) {
        FURI_LOG_E(TAG, "Failed to allocate RX thread");
        goto cleanup;
    }

    furi_thread_set_name(sio->rx_thread, "SIO RX");
    furi_thread_set_stack_size(sio->rx_thread, 4096);
    furi_thread_set_context(sio->rx_thread, sio);
    furi_thread_set_callback(sio->rx_thread, sio_driver_rx_worker);

    furi_thread_start(sio->rx_thread);

    furi_hal_serial_async_rx_start(sio->uart, sio_driver_rx_callback, sio, true);

    furi_hal_gpio_add_int_callback(GPIO_EXT_COMMAND, sio_driver_command_pin_callback, sio);

    FURI_LOG_I(TAG, "SIO driver initialized");

    return sio;

cleanup:
    sio_driver_free(sio);
    return NULL;
}

void sio_driver_free(SIODriver* sio) {
    if(sio != NULL) {
        furi_hal_gpio_remove_int_callback(GPIO_EXT_COMMAND);

        if(sio->uart != NULL) {
            furi_hal_serial_async_rx_stop(sio->uart);
        }

        if(sio->rx_thread != NULL) {
            furi_thread_flags_set(furi_thread_get_id(sio->rx_thread), WORKER_EVT_STOP);
            furi_thread_join(sio->rx_thread);
            furi_thread_free(sio->rx_thread);
        }

        if(sio->uart != NULL) {
            furi_hal_serial_deinit(sio->uart);
            furi_hal_serial_control_release(sio->uart);
        }

        free(sio);

        furi_hal_gpio_init(GPIO_EXT_COMMAND, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_init(GPIO_EXT_MOTORCTL, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

        FURI_LOG_I(TAG, "SIO driver deinitialized");
    }
}

bool sio_driver_attach(
    SIODriver* sio,
    SIODevice device,
    SIOCallback command_callback,
    SIOCallback data_callback,
    void* context) {
    furi_check(sio != NULL);
    furi_check(command_callback != NULL);
    furi_check(data_callback != NULL);

    for(int i = 0; i < MAX_SIO_DEVICES; i++) {
        SIOHandler* handler = &sio->handler[i];
        if(handler->command_callback == NULL) {
            handler->device = device;
            handler->command_callback = command_callback;
            handler->data_callback = data_callback;
            handler->callback_context = context;
            return true;
        }
    }

    return false;
}

void sio_driver_detach(SIODriver* sio, uint8_t device) {
    furi_check(sio != NULL);

    for(int i = 0; i < MAX_SIO_DEVICES; i++) {
        SIOHandler* handler = &sio->handler[i];
        if(handler->device == device) {
            handler->device = 0;
            handler->command_callback = NULL;
            handler->data_callback = NULL;
            handler->callback_context = NULL;
            break;
        }
    }
}
