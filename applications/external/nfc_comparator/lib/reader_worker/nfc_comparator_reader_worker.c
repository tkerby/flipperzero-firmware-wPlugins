#include "nfc_comparator_reader_worker.h"

NfcComparatorReaderWorker* nfc_comparator_reader_worker_alloc() {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker =
      malloc(sizeof(NfcComparatorReaderWorker));
   nfc_comparator_reader_worker->nfc = nfc_alloc();
   nfc_comparator_reader_worker->thread = furi_thread_alloc_ex(
      "NfcComparatorReaderWorker",
      1024,
      nfc_comparator_reader_worker_task,
      nfc_comparator_reader_worker);
   nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Stopped;
   return nfc_comparator_reader_worker;
}

void nfc_comparator_reader_worker_free(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   nfc_free(nfc_comparator_reader_worker->nfc);
   furi_thread_free(nfc_comparator_reader_worker->thread);
   nfc_device_free(nfc_comparator_reader_worker->loaded_nfc_card);
   if(nfc_comparator_reader_worker->scanned_nfc_card) {
      nfc_device_free(nfc_comparator_reader_worker->scanned_nfc_card);
   }
   free(nfc_comparator_reader_worker);
}

void nfc_comparator_reader_worker_stop(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   if(nfc_comparator_reader_worker->state != NfcComparatorReaderWorkerState_Stopped) {
      nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Stopped;
      furi_thread_join(nfc_comparator_reader_worker->thread);
   }
}

void nfc_comparator_reader_worker_start(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   if(nfc_comparator_reader_worker->state == NfcComparatorReaderWorkerState_Stopped) {
      nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Scanning;
      furi_thread_start(nfc_comparator_reader_worker->thread);
   }
}

void nfc_comparator_reader_worker_scanner_callback(NfcScannerEvent event, void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
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
   furi_assert(nfc_comparator_reader_worker);
   UNUSED(event);
   NfcCommand command = NfcCommandStop;
   nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Comparing;
   return command;
}

int32_t nfc_comparator_reader_worker_task(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   while(nfc_comparator_reader_worker->state != NfcComparatorReaderWorkerState_Stopped) {
      switch(nfc_comparator_reader_worker->state) {
      case NfcComparatorReaderWorkerState_Scanning: {
         NfcScanner* nfc_scanner = nfc_scanner_alloc(nfc_comparator_reader_worker->nfc);
         nfc_scanner_start(
            nfc_scanner,
            nfc_comparator_reader_worker_scanner_callback,
            nfc_comparator_reader_worker);
         while(nfc_comparator_reader_worker->state == NfcComparatorReaderWorkerState_Scanning) {
            furi_delay_ms(100);
         }
         nfc_scanner_stop(nfc_scanner);
         break;
      }
      case NfcComparatorReaderWorkerState_Polling: {
         NfcPoller* nfc_poller = nfc_poller_alloc(
            nfc_comparator_reader_worker->nfc, nfc_comparator_reader_worker->protocol[0]);
         nfc_poller_start(
            nfc_poller,
            nfc_comparator_reader_worker_poller_callback,
            nfc_comparator_reader_worker);
         while(nfc_comparator_reader_worker->state == NfcComparatorReaderWorkerState_Polling) {
            furi_delay_ms(100);
         }
         nfc_poller_stop(nfc_poller);
         nfc_comparator_reader_worker->scanned_nfc_card = nfc_device_alloc();
         nfc_device_set_data(
            nfc_comparator_reader_worker->scanned_nfc_card,
            nfc_comparator_reader_worker->protocol[0],
            (NfcDeviceData*)nfc_poller_get_data(nfc_poller));
         nfc_poller_free(nfc_poller);
         break;
      }
      case NfcComparatorReaderWorkerState_Comparing: {
         nfc_comparator_reader_worker->compare_checks.protocol =
            nfc_device_get_protocol_name(
               nfc_device_get_protocol(nfc_comparator_reader_worker->scanned_nfc_card)) ==
                  nfc_device_get_protocol_name(
                     nfc_device_get_protocol(nfc_comparator_reader_worker->loaded_nfc_card)) ?
               true :
               false;

         size_t poller_uid_len = 0;
         const uint8_t* poller_uid =
            nfc_device_get_uid(nfc_comparator_reader_worker->scanned_nfc_card, &poller_uid_len);
         FuriString* poller_uid_str = furi_string_alloc();
         for(size_t i = 0; i < poller_uid_len; i++) {
            furi_string_utf8_push(poller_uid_str, poller_uid[i]);
         }

         nfc_device_free(nfc_comparator_reader_worker->scanned_nfc_card);

         size_t loaded_uid_len = 0;
         const uint8_t* loaded_uid =
            nfc_device_get_uid(nfc_comparator_reader_worker->loaded_nfc_card, &loaded_uid_len);
         FuriString* loaded_uid_str = furi_string_alloc();
         for(size_t i = 0; i < loaded_uid_len; i++) {
            furi_string_utf8_push(loaded_uid_str, loaded_uid[i]);
         }

         nfc_comparator_reader_worker->compare_checks.uid =
            (furi_string_cmpi(loaded_uid_str, poller_uid_str) == 0);

         nfc_comparator_reader_worker->compare_checks.uid_length =
            (poller_uid_len == loaded_uid_len);

         furi_string_free(poller_uid_str);
         furi_string_free(loaded_uid_str);

         nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Stopped;
         break;
      }
      default:
         break;
      }
   }
   return 0;
}

bool nfc_comparator_reader_worker_set_loaded_nfc_card(void* context, const char* path_to_nfc_card) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   nfc_comparator_reader_worker->loaded_nfc_card = nfc_device_alloc();
   return nfc_device_load(nfc_comparator_reader_worker->loaded_nfc_card, path_to_nfc_card);
}

NfcComparatorReaderWorkerState nfc_comparator_reader_worker_get_state(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   return nfc_comparator_reader_worker->state;
}

NfcComparatorReaderWorkerCompareChecks
   nfc_comparator_reader_worker_get_compare_checks(void* context) {
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   furi_assert(nfc_comparator_reader_worker);
   return nfc_comparator_reader_worker->compare_checks;
}
