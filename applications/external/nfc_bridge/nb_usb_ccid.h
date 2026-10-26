/*
 * nb_usb_ccid.h - a USB CCID (smart-card reader) device for the Flipper's USB stack.
 *
 * The firmware has CDC, HID and U2F interfaces built in but no CCID one, so this
 * app brings its own FuriHalUsbInterface (descriptors + endpoint handling), built
 * on the same libusb_stm32 calls the CDC interface uses.
 *
 * Endpoints: bulk OUT 0x01, bulk IN 0x82, interrupt IN 0x83.
 * The callbacks run in the USB driver's context: only set flags / release
 * semaphores in them, do the real work in your own thread.
 */
#pragma once
#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

#define NB_USB_CCID_PKT 64

/* Placeholder IDs (ST's vendor id, like the firmware's own CDC device). Change them
 * here AND in pc/install_ccid_linux.sh if you want something else. */
#define NB_USB_VID 0x0483
#define NB_USB_PID 0x5748

typedef struct {
    void (*rx)(void* ctx); /* a bulk OUT packet has arrived */
    void (*tx_done)(void* ctx); /* the last bulk IN packet was taken by the host */
    void (*connected)(void* ctx, bool on); /* host woke up / suspended the bus */
} NbUsbCcidCallbacks;

/* Switch the Flipper's USB port to the CCID device. Remembers the previous mode. */
bool nb_usb_ccid_start(const NbUsbCcidCallbacks* cb, void* ctx);

/* Go back to the previous USB mode. */
void nb_usb_ccid_stop(void);

/* Read one packet (<= 64 bytes) from bulk OUT. Returns bytes read, 0 if none. */
int32_t nb_usb_ccid_read(uint8_t* buf, uint16_t max);

/* Queue one packet (<= 64 bytes, 0 = zero-length packet) on bulk IN. */
int32_t nb_usb_ccid_write(const uint8_t* buf, uint16_t len);

/* Queue an interrupt IN message (RDR_to_PC_NotifySlotChange). */
int32_t nb_usb_ccid_notify(const uint8_t* buf, uint16_t len);
