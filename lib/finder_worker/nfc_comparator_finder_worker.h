#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <storage/storage.h>
#include <dir_walk.h>
#include <path.h>

#include "../compare_checks/nfc_comparator_compare_checks.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Possible states for the NFC Comparator Reader Worker */
typedef enum {
   NfcComparatorFinderWorkerState_Scanning,
   NfcComparatorFinderWorkerState_Polling,
   NfcComparatorFinderWorkerState_Finding,
   NfcComparatorFinderWorkerState_Stopped
} NfcComparatorFinderWorkerState;

/** Holds settings for the NFC Comparator Finder Worker */
typedef struct {
   bool recursive; /**< Whether to search recursively */
} NfcComparatorFinderWorkerSettings;

/** Holds all state for the NFC Comparator Reader Worker */
typedef struct {
   Nfc* nfc;
   FuriThread* thread;
   NfcProtocol* protocol;
   NfcComparatorFinderWorkerState state;
   NfcDevice* loaded_nfc_card;
   NfcDevice* scanned_nfc_card;
   NfcPoller* nfc_poller;
   DirWalk* dir_walk;
   NfcComparatorCompareChecks* compare_checks;
   NfcComparatorFinderWorkerSettings* settings;
} NfcComparatorFinderWorker;

NfcComparatorFinderWorker* nfc_comparator_finder_worker_alloc(
   NfcComparatorCompareChecks* compare_checks,
   NfcComparatorFinderWorkerSettings* settings);

void nfc_comparator_finder_worker_free(NfcComparatorFinderWorker* worker);

void nfc_comparator_finder_worker_stop(NfcComparatorFinderWorker* worker);

void nfc_comparator_finder_worker_start(NfcComparatorFinderWorker* worker);

NfcComparatorFinderWorkerState
   nfc_comparator_finder_worker_get_state(const NfcComparatorFinderWorker* worker);

void nfc_comparator_finder_worker_compare_cards(
   NfcComparatorCompareChecks* compare_checks,
   NfcDevice* nfc_card_1,
   bool check_data,
   NfcComparatorFinderWorkerSettings* settings,
   FuriString* nfc_card_path);

#ifdef __cplusplus
}
#endif
