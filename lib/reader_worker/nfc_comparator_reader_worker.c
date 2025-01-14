#include "nfc_comparator_reader_worker.h"

NfcComparatorReaderWorker* nfc_comparator_reader_worker_alloc() {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker =
      malloc(sizeof(NfcComparatorReaderWorker));
   nfc_comparator_reader_worker->nfc = nfc_alloc();
   nfc_comparator_reader_worker->nfc_scanner =
      nfc_scanner_alloc(nfc_comparator_reader_worker->nfc);
   nfc_comparator_reader_worker->thread = furi_thread_alloc_ex(
      "NfcComparatorReaderWorker",
      4096,
      nfc_comparator_reader_worker_task,
      nfc_comparator_reader_worker);
   return nfc_comparator_reader_worker;
}

void nfc_comparator_reader_worker_free(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   nfc_free(nfc_comparator_reader_worker->nfc);
   nfc_device_free(nfc_comparator_reader_worker->loaded_nfc_card);
   nfc_scanner_free(nfc_comparator_reader_worker->nfc_scanner);
   furi_thread_free(nfc_comparator_reader_worker->thread);
   free(nfc_comparator_reader_worker);
}

void nfc_comparator_reader_worker_scanner_callback(NfcScannerEvent event, void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   switch(event.type) {
   case NfcScannerEventTypeDetected:
      nfc_comparator_reader_worker->protocol = event.data.protocols;
      nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Polling;
      break;
   default:
      break;
   }
}

NfcCommand nfc_comparator_reader_worker_poller_callback(NfcGenericEvent event, void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   UNUSED(event);
   NfcCommand command = NfcCommandStop;
   nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Comparing;
   return command;
}

int32_t nfc_comparator_reader_worker_task(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   nfc_comparator_reader_worker->nfc_scanner =
      nfc_scanner_alloc(nfc_comparator_reader_worker->nfc);

   nfc_scanner_start(
      nfc_comparator_reader_worker->nfc_scanner,
      nfc_comparator_reader_worker_scanner_callback,
      nfc_comparator_reader_worker);

   nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Scanning;
   while(nfc_comparator_reader_worker->state == NfcComparatorReaderWorkerState_Scanning) {
      furi_delay_ms(100);
   }
   nfc_scanner_stop(nfc_comparator_reader_worker->nfc_scanner);

   NfcPoller* nfc_poller = nfc_poller_alloc(
      nfc_comparator_reader_worker->nfc, nfc_comparator_reader_worker->protocol[0]);

   nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Polling;
   nfc_poller_start(
      nfc_poller, nfc_comparator_reader_worker_poller_callback, nfc_comparator_reader_worker);

   while(nfc_comparator_reader_worker->state == NfcComparatorReaderWorkerState_Polling) {
      furi_delay_ms(100);
   }
   nfc_poller_stop(nfc_poller);

   NfcDeviceData* nfc_device_data = (NfcDeviceData*)nfc_poller_get_data(nfc_poller);
   NfcDevice* data = nfc_device_alloc();
   nfc_device_set_data(data, nfc_comparator_reader_worker->protocol[0], nfc_device_data);

   if(nfc_device_get_protocol_name(nfc_device_get_protocol(data)) ==
      nfc_device_get_protocol_name(
         nfc_device_get_protocol(nfc_comparator_reader_worker->loaded_nfc_card))) {
      nfc_comparator_reader_worker->compare_checks->protocol = true;
   } else {
      nfc_comparator_reader_worker->compare_checks->protocol = false;
   }

   nfc_poller_free(nfc_poller);

   size_t poller_uid_len = 0;
   const uint8_t* uid = nfc_device_get_uid(data, &poller_uid_len);
   FuriString* poller_uid_str = furi_string_alloc();
   for(size_t i = 0; i < poller_uid_len; i++) {
      char uid_str[3];
      snprintf(uid_str, sizeof(uid_str), "%02X", uid[i]);
      furi_string_cat(poller_uid_str, uid_str);
   }

   size_t loaded_uid_len = 0;
   const uint8_t* loaded_uid =
      nfc_device_get_uid(nfc_comparator_reader_worker->loaded_nfc_card, &loaded_uid_len);
   FuriString* loaded_uid_str = furi_string_alloc();
   for(size_t i = 0; i < loaded_uid_len; i++) {
      char uid_str[3];
      snprintf(uid_str, sizeof(uid_str), "%02X", loaded_uid[i]);
      furi_string_cat(loaded_uid_str, uid_str);
   }

   if(furi_string_cmpi(poller_uid_str, loaded_uid_str) == 0) {
      nfc_comparator_reader_worker->compare_checks->uid = true;
   } else {
      nfc_comparator_reader_worker->compare_checks->uid = false;
   }

   furi_string_free(poller_uid_str);
   furi_string_free(loaded_uid_str);

   return 0;
}

void nfc_comparator_reader_worker_set_compare_nfc_device(void* context, NfcDevice* nfc_device) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   nfc_comparator_reader_worker->loaded_nfc_card = nfc_device;
}

bool nfc_comparator_reader_worker_is_done(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   return nfc_comparator_reader_worker->state == NfcComparatorReaderWorkerState_Stopped;
}

NfcComparatorReaderWorkerCompareChecks*
   nfc_comparator_reader_worker_get_compare_checks(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   return nfc_comparator_reader_worker->compare_checks;
}

void nfc_comparator_reader_worker_start(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   furi_thread_start(nfc_comparator_reader_worker->thread);
}

void nfc_comparator_reader_worker_stop(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Stopped;
   furi_thread_join(nfc_comparator_reader_worker->thread);
}
