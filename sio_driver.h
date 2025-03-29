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

#pragma once

#define SIO_MAX_FRAME_SIZE 1024U

// SIO status codes
typedef enum {
    SIO_NORESP = 0x00,
    SIO_ACK = 0x41, // 'A'
    SIO_NAK = 0x4E, // 'N'
    SIO_COMPLETE = 0x43, // 'C'
    SIO_ERROR = 0x45, // 'E'
} SIOStatus;

// SIO device ids
typedef enum {
    SIO_DEVICE_DISK1 = 0x31,
    SIO_DEVICE_DISK2 = 0x32,
    SIO_DEVICE_DISK3 = 0x33,
    SIO_DEVICE_DISK4 = 0x34,
} SIODevice;

// SIO command codes
typedef enum {
    SIO_COMMAND_READ = 0x52, // Read sector
    SIO_COMMAND_WRITE = 0x57, // Write sector
    SIO_COMMAND_STATUS = 0x53, // Get status
    SIO_COMMAND_PUT = 0x50, // Write sector without verification
    SIO_COMMAND_FORMAT = 0x21, // Format disk
    SIO_COMMAND_FORMAT_MEDIUM = 0x22, // Format disk (medium density)
    SIO_COMMAND_GET_HSI = 0x3F, // Get high-speed index
    SIO_COMMAND_READ_PERCOM = 0x4E, // Read PERCOM configuration
    SIO_COMMAND_WRITE_PERCOM = 0x4F, // Write PERCOM configuration
    SIO_COMMAND_FORMAT_WITH_SKEW = 0x66, // Format disk with skew

    SIO_COMMAND_READ_HS = 0xD2, // Read sector
    SIO_COMMAND_WRITE_HS = 0xD7, // Write sector
    SIO_COMMAND_STATUS_HS = 0xD3, // Get status
    SIO_COMMAND_PUT_HS = 0xD0, // Write sector without verification
    SIO_COMMAND_FORMAT_HS = 0xA1, // Format disk

    SIO_COMMAND_READ_BLOCK_HEADER = 0xF0, // Read XEX block header
    SIO_COMMAND_READ_BLOCK = 0xF1, // Read XEX block

} SIOCommand;

// SIO request structure passed to the device driver
typedef struct {
    // Device id
    SIODevice device;
    // Command
    SIOCommand command;
    // Auxilary data (command specific)
    uint8_t aux1;
    uint8_t aux2;

    // Received data (set by device driver)
    uint8_t rx_data[SIO_MAX_FRAME_SIZE];
    // Received data size (set by device driver)
    size_t rx_size;

    // Data to send (set by device driver)
    uint8_t tx_data[SIO_MAX_FRAME_SIZE];
    // Size of data to send (set by device driver)
    size_t tx_size;

    // baudrate for data frame
    uint32_t baudrate;

} SIORequest;

// Callback for processing SIO requests
//
// NOTE: The callback is invoked from the SIO driver thread.
//
// There are two phases in SIO request processing:
//
//   Command frame phase:
//    - Triggered when a command frame is received.
//    - The callback must respond quickly, within 16ms.
//    - The callback should process the command, aux1, and aux2.
//    - Optionally, the callback can set `rx_size` to receive data from the host.
//    - Optionally, the callback can update `baudrate` for the data frame phase.
//    - The callback should return either `SIO_ACK` or `SIO_NAK`.
//
//   Data frame phase:
//    - Triggered when a data frame is received or sent.
//    - The callback must process the received data or prepare data to send.
//    - Optionally, the callback can set `tx_data` and `tx_size`.
//    - The callback should return `SIO_COMPLETE` or `SIO_ERROR`.
typedef SIOStatus (*SIOCallback)(void* context, SIORequest* request);

// SIO driver
typedef struct SIODriver SIODriver;

// Allocates an SIO driver and initializes the UART
SIODriver* sio_driver_alloc(void);

// Frees the SIO driver
void sio_driver_free(SIODriver* sio);

// Attaches a peripheral driver to the SIO driver
bool sio_driver_attach(
    SIODriver* sio,
    SIODevice device,
    SIOCallback command_callback,
    SIOCallback data_callback,
    void* context);

// Detaches a periheral driver from the SIO driver
void sio_driver_detach(SIODriver* sio, uint8_t device);
