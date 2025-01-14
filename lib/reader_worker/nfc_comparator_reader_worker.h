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
   bool uid;
   bool protocol;
} NfcComparatorReaderWorkerCompareChecks;

typedef struct {
   Nfc* nfc;
   FuriThread* thread;
   NfcProtocol* protocol;
   NfcComparatorReaderWorkerState state;
   NfcDevice* loaded_nfc_card;
   NfcPoller* nfc_poller;
   NfcScanner* nfc_scanner;
   NfcComparatorReaderWorkerCompareChecks compare_checks;
} NfcComparatorReaderWorker;

NfcComparatorReaderWorker* nfc_comparator_reader_worker_alloc();
void nfc_comparator_reader_worker_free(void* context);

void nfc_comparator_reader_worker_scanner_callback(NfcScannerEvent event, void* context);
NfcCommand nfc_comparator_reader_worker_poller_callback(NfcGenericEvent event, void* context);
int32_t nfc_comparator_reader_worker_task(void* context);
void nfc_comparator_reader_worker_set_compare_nfc_device(void* context, NfcDevice* nfc_device);