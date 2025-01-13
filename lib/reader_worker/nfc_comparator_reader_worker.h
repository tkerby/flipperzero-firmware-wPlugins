#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>

typedef enum {
   NfcComparatorReaderWorkerState_Scanning,
   NfcComparatorReaderWorkerState_Polling,
   NfcComparatorReaderWorkerState_Stopped
} NfcComparatorReaderWorkerState;

typedef struct {
   Nfc* nfc;
   FuriThread* thread;
   NfcProtocol protocol;
   NfcComparatorReaderWorkerState state;
   NfcDevice* nfc_device;
   NfcPoller* nfc_poller;
   NfcScanner* nfc_scanner;
} NfcComparatorReaderWorker;

NfcComparatorReaderWorker* nfc_comparator_reader_worker_alloc();
void nfc_comparator_reader_worker_free(NfcComparatorReaderWorker* nfc_comparator_reader_worker);
