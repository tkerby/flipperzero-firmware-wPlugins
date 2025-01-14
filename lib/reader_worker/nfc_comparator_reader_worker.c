#include "nfc_comparator_reader_worker.h"

NfcComparatorReaderWorker* nfc_comparator_reader_worker_alloc() {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker =
      malloc(sizeof(NfcComparatorReaderWorker));
   nfc_comparator_reader_worker->nfc = nfc_alloc();
   nfc_comparator_reader_worker->nfc_scanner =
      nfc_scanner_alloc(nfc_comparator_reader_worker->nfc);
   return nfc_comparator_reader_worker;
}

void nfc_comparator_reader_worker_free(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   nfc_free(nfc_comparator_reader_worker->nfc);
   nfc_device_free(nfc_comparator_reader_worker->loaded_nfc_card);
   nfc_scanner_free(nfc_comparator_reader_worker->nfc_scanner);
   free(nfc_comparator_reader_worker);
}

void nfc_comparator_reader_worker_scanner_callback(NfcScannerEvent event, void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   switch(event.type) {
   case NfcScannerEventTypeDetected:
      FURI_LOG_RAW_I("\nNFC Scanner Event Detected");
      nfc_comparator_reader_worker->protocol = event.data.protocols;
      FURI_LOG_RAW_I(
         "Detected protocol: %s",
         nfc_device_get_protocol_name(nfc_comparator_reader_worker->protocol[0]));
      nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Polling;
      break;
   default:
      FURI_LOG_RAW_I("\nNFC Scanner Event Type: %d", event.type);
      break;
   }
}

NfcCommand nfc_comparator_reader_worker_poller_callback(NfcGenericEvent event, void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   UNUSED(event);
   NfcCommand command = NfcCommandStop;
   nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Stopped;
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

   if (nfc_device_get_protocol_name(nfc_device_get_protocol(data)) == nfc_device_get_protocol_name(nfc_device_get_protocol(nfc_comparator_reader_worker->loaded_nfc_card))) {
      FURI_LOG_RAW_I("\nProtocol is equal");
   } else {
      FURI_LOG_RAW_I("\nProtocol is not equal");
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
   FURI_LOG_RAW_I("\nUID: %s", furi_string_get_cstr(poller_uid_str));

   furi_string_free(poller_uid_str);

   size_t loaded_uid_len = 0;
   const uint8_t* loaded_uid = nfc_device_get_uid(nfc_comparator_reader_worker->loaded_nfc_card, &loaded_uid_len);
   FuriString* loaded_uid_str = furi_string_alloc();
   for(size_t i = 0; i < loaded_uid_len; i++) {
      char uid_str[3];
      snprintf(uid_str, sizeof(uid_str), "%02X", loaded_uid[i]);
      furi_string_cat(loaded_uid_str, uid_str);
   }
   FURI_LOG_RAW_I("\nUID: %s", furi_string_get_cstr(loaded_uid_str));

   furi_string_free(loaded_uid_str);

   return 0;
}


void nfc_comparator_reader_worker_set_compare_nfc_device(void* context, NfcDevice* nfc_device) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   nfc_comparator_reader_worker->loaded_nfc_card = nfc_device;
}
