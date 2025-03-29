#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>

typedef enum {
   NfcComparatorReaderWorkerState_Scanning,
   NfcComparatorReaderWorkerState_Polling,
   NfcComparatorReaderWorkerState_Comparing,
   NfcComparatorReaderWorkerState_Stopped
} NfcComparatorReaderWorkerState;

typedef struct {
   bool uid;
   bool uid_length;
   bool protocol;
} NfcComparatorReaderWorkerCompareChecks;

typedef struct {
   Nfc* nfc;
   FuriThread* thread;
   NfcProtocol* protocol;
   NfcComparatorReaderWorkerState state;
   NfcDevice* loaded_nfc_card;
   NfcDevice* scanned_nfc_card;
   NfcPoller* nfc_poller;
   NfcComparatorReaderWorkerCompareChecks compare_checks;
} NfcComparatorReaderWorker;

NfcComparatorReaderWorker* nfc_comparator_reader_worker_alloc();
void nfc_comparator_reader_worker_free(void* context);
void nfc_comparator_reader_worker_stop(void* context);
void nfc_comparator_reader_worker_start(void* context);

void nfc_comparator_reader_worker_scanner_callback(NfcScannerEvent event, void* context);
NfcCommand nfc_comparator_reader_worker_poller_callback(NfcGenericEvent event, void* context);

int32_t nfc_comparator_reader_worker_task(void* context);

bool nfc_comparator_reader_worker_set_loaded_nfc_card(void* context, const char* path_to_nfc_card);
NfcComparatorReaderWorkerState nfc_comparator_reader_worker_get_state(void* context);
NfcComparatorReaderWorkerCompareChecks
   nfc_comparator_reader_worker_get_compare_checks(void* context);
