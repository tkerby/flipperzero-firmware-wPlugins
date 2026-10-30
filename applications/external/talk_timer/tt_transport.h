#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "tt_settings.h"

/*
 * Transport-Schicht: USB-HID oder Bluetooth-HID hinter einer gemeinsamen API.
 * Vorlage: applications/system/hid_app (Momentum, mntm-012), dort transport_usb.c
 * und transport_ble.c. Hier zur Laufzeit wählbar statt zur Compile-Zeit.
 */
typedef struct TtTransport TtTransport;

/*
 * Startet den Transport (USB-Modus umschalten bzw. BLE-Profil starten).
 * Kein Status-Callback: Der USB-HID-Callback der Firmware läuft im Interrupt
 * (usbd_poll im USB_LP_IRQHandler), dort darf kein ViewDispatcher-Event
 * abgesetzt werden. Der Status wird deshalb wie in der HID-App per
 * tt_transport_is_connected() zyklisch abgefragt.
 */
TtTransport* tt_transport_alloc(TtConn conn);

/* Stoppt den Transport und stellt den Ursprungszustand wieder her */
void tt_transport_free(TtTransport* transport);

bool tt_transport_is_connected(TtTransport* transport);

/* Taste (inkl. Modifier-Bits) drücken / loslassen */
bool tt_transport_press(TtTransport* transport, uint16_t hid_code);
bool tt_transport_release(TtTransport* transport, uint16_t hid_code);

/* Löscht die gespeicherte Bluetooth-Kopplung der App (nur bei gestopptem Transport) */
void tt_transport_forget_bt_pairing(void);

/*
 * Bewusst KEIN blockierendes "tap" mit furi_delay: Die App drückt die Taste und
 * lässt sie zeitversetzt über einen FuriTimer wieder los (siehe talk_timer.c).
 */
