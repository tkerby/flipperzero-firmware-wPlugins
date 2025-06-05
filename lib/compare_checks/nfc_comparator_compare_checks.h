#pragma once

#include <furi.h>
#include <nfc/nfc_device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   NfcCompareChecksType_Digital,
   NfcCompareChecksType_Physical,
   NfcCompareChecksType_Undefined
} NfcCompareChecksType;

typedef struct {
   FuriString* nfc_card_path; /**< Path to the NFC file */
   bool uid; /**< Compare UID */
   bool uid_length; /**< Compare UID length */
   bool protocol; /**< Compare protocol */
   bool nfc_data; /**< Compare NFC data */
   NfcCompareChecksType type; /**< Type of comparison checks */
} NfcComparatorCompareChecks;

NfcComparatorCompareChecks* nfc_comparator_compare_checks_alloc();

void nfc_comparator_compare_checks_free(NfcComparatorCompareChecks* checks);

void nfc_comparator_compare_checks_reset(NfcComparatorCompareChecks* checks);

NfcCompareChecksType nfc_comparator_compare_checks_get_type(NfcComparatorCompareChecks* checks);

void nfc_comparator_compare_checks_set_type(
   NfcComparatorCompareChecks* checks,
   NfcCompareChecksType type);

void nfc_comparator_compare_checks_compare_cards(
   NfcComparatorCompareChecks* checks,
   NfcDevice* nfc_card_1,
   NfcDevice* nfc_card_2,
   bool check_data);

#ifdef __cplusplus
}
#endif
