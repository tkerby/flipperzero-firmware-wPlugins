#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <storage/storage.h>
#include <dir_walk.h>
#include <path.h>

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

/** Comparison checks for NFC cards */
typedef struct {
   FuriString* nfc_card_path; /**< Path to the NFC file */
   bool uid; /**< Compare UID */
   bool uid_length; /**< Compare UID length */
   bool protocol; /**< Compare protocol */
   bool nfc_data; /**< Compare NFC data */
} NfcComparatorFinderWorkerCompareChecks;

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
   NfcComparatorFinderWorkerCompareChecks* compare_checks;
   NfcComparatorFinderWorkerSettings* settings;
} NfcComparatorFinderWorker;

NfcComparatorFinderWorker* nfc_comparator_finder_worker_alloc(
   NfcComparatorFinderWorkerCompareChecks* compare_checks,
   NfcComparatorFinderWorkerSettings* settings);

void nfc_comparator_finder_worker_free(NfcComparatorFinderWorker* worker);

void nfc_comparator_finder_worker_stop(NfcComparatorFinderWorker* worker);

void nfc_comparator_finder_worker_start(NfcComparatorFinderWorker* worker);

NfcComparatorFinderWorkerState
   nfc_comparator_finder_worker_get_state(const NfcComparatorFinderWorker* worker);

void nfc_comparator_finder_worker_compare_cards(
   NfcComparatorFinderWorkerCompareChecks* compare_checks,
   NfcDevice* card1,
   NfcDevice* card2,
   bool check_data);

#ifdef __cplusplus
}
#endif
