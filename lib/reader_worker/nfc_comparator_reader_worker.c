#include "nfc_comparator_reader_worker.h"

NfcComparatorReaderWorker* nfc_comparator_reader_worker_alloc() {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker =
      malloc(sizeof(NfcComparatorReaderWorker));
   furi_assert(nfc_comparator_reader_worker);
   nfc_comparator_reader_worker->nfc = nfc_alloc();
   nfc_comparator_reader_worker->nfc_device = nfc_device_alloc();
   nfc_comparator_reader_worker->nfc_scanner =
      nfc_scanner_alloc(nfc_comparator_reader_worker->nfc);
   return nfc_comparator_reader_worker;
}

void nfc_comparator_reader_worker_free(NfcComparatorReaderWorker* nfc_comparator_reader_worker) {
   furi_assert(nfc_comparator_reader_worker);
   nfc_free(nfc_comparator_reader_worker->nfc);
   nfc_device_free(nfc_comparator_reader_worker->nfc_device);
   nfc_scanner_free(nfc_comparator_reader_worker->nfc_scanner);
   free(nfc_comparator_reader_worker);
}

void nfc_comparator_reader_worker_start(NfcComparatorReaderWorker* nfc_comparator_reader_worker) {
   furi_assert(nfc_comparator_reader_worker);
   nfc_poller_start(nfc_comparator_reader_worker->nfc_poller);
}
