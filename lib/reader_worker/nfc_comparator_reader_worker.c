#include "nfc_comparator_reader_worker.h"

static void nfc_comparator_reader_worker_scanner_callback(NfcScannerEvent event, void* context) {
   furi_assert(context);
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

static NfcCommand
   nfc_comparator_reader_worker_poller_callback(NfcGenericEvent event, void* context) {
   furi_assert(context);
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   UNUSED(event);
   nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Comparing;
   return NfcCommandStop;
}

static int32_t nfc_comparator_reader_worker_task(void* context) {
   NfcComparatorReaderWorker* worker = context;
   furi_assert(worker);
   while(worker->state != NfcComparatorReaderWorkerState_Stopped) {
      switch(worker->state) {
      case NfcComparatorReaderWorkerState_Scanning: {
         NfcScanner* nfc_scanner = nfc_scanner_alloc(worker->nfc);
         nfc_scanner_start(nfc_scanner, nfc_comparator_reader_worker_scanner_callback, worker);
         while(worker->state == NfcComparatorReaderWorkerState_Scanning) {
            furi_delay_ms(100);
         }
         nfc_scanner_stop(nfc_scanner);
         break;
      }
      case NfcComparatorReaderWorkerState_Polling: {
         NfcPoller* nfc_poller = nfc_poller_alloc(worker->nfc, worker->protocol[0]);
         nfc_poller_start(nfc_poller, nfc_comparator_reader_worker_poller_callback, worker);
         while(worker->state == NfcComparatorReaderWorkerState_Polling) {
            furi_delay_ms(100);
         }
         nfc_poller_stop(nfc_poller);
         worker->scanned_nfc_card = nfc_device_alloc();
         nfc_device_set_data(
            worker->scanned_nfc_card,
            worker->protocol[0],
            (NfcDeviceData*)nfc_poller_get_data(nfc_poller));
         nfc_poller_free(nfc_poller);
         break;
      }
      case NfcComparatorReaderWorkerState_Comparing: {
         nfc_comparator_reader_worker_compare_cards(
            worker->compare_checks, worker->scanned_nfc_card, worker->loaded_nfc_card, false);

         nfc_device_free(worker->scanned_nfc_card);
         worker->scanned_nfc_card = NULL;
         nfc_device_free(worker->loaded_nfc_card);
         worker->loaded_nfc_card = NULL;

         worker->state = NfcComparatorReaderWorkerState_Stopped;
         break;
      }
      default:
         break;
      }
   }
   return 0;
}

NfcComparatorReaderWorker*
   nfc_comparator_reader_worker_alloc(NfcComparatorReaderWorkerCompareChecks* compare_checks) {
   NfcComparatorReaderWorker* worker = calloc(1, sizeof(NfcComparatorReaderWorker));
   if(!worker) return NULL;
   worker->nfc = nfc_alloc();
   if(!worker->nfc) {
      free(worker);
      return NULL;
   }

   worker->compare_checks = compare_checks;

   worker->thread = furi_thread_alloc();
   furi_thread_set_name(worker->thread, "NfcComparatorReaderWorker");
   furi_thread_set_context(worker->thread, worker);
   furi_thread_set_stack_size(worker->thread, 1024);
   furi_thread_set_callback(worker->thread, nfc_comparator_reader_worker_task);

   if(!worker->thread) {
      nfc_free(worker->nfc);
      free(worker);
      return NULL;
   }
   worker->state = NfcComparatorReaderWorkerState_Stopped;
   worker->loaded_nfc_card = NULL;
   worker->scanned_nfc_card = NULL;
   worker->protocol = NULL;
   return worker;
}

void nfc_comparator_reader_worker_free(NfcComparatorReaderWorker* worker) {
   furi_assert(worker);
   nfc_free(worker->nfc);
   furi_thread_free(worker->thread);
   if(worker->loaded_nfc_card) {
      nfc_device_free(worker->loaded_nfc_card);
      worker->loaded_nfc_card = NULL;
   }
   if(worker->scanned_nfc_card) {
      nfc_device_free(worker->scanned_nfc_card);
      worker->scanned_nfc_card = NULL;
   }
   free(worker);
}

void nfc_comparator_reader_stop(NfcComparatorReaderWorker* worker) {
   furi_assert(worker);
   if(worker->state != NfcComparatorReaderWorkerState_Stopped) {
      worker->state = NfcComparatorReaderWorkerState_Stopped;
      furi_thread_join(worker->thread);
   }
}

void nfc_comparator_reader_start(NfcComparatorReaderWorker* worker) {
   furi_assert(worker);
   if(worker->state == NfcComparatorReaderWorkerState_Stopped) {
      worker->state = NfcComparatorReaderWorkerState_Scanning;
      furi_thread_start(worker->thread);
   }
}

bool nfc_comparator_reader_worker_set_loaded_nfc_card(
   NfcComparatorReaderWorker* worker,
   const char* path_to_nfc_card) {
   furi_assert(worker);
   if(worker->loaded_nfc_card) {
      nfc_device_free(worker->loaded_nfc_card);
      worker->loaded_nfc_card = NULL;
   }
   worker->loaded_nfc_card = nfc_device_alloc();
   if(!worker->loaded_nfc_card) return false;
   return nfc_device_load(worker->loaded_nfc_card, path_to_nfc_card);
}

NfcComparatorReaderWorkerState
   nfc_comparator_reader_worker_get_state(const NfcComparatorReaderWorker* worker) {
   furi_assert(worker);
   return worker->state;
}

void nfc_comparator_reader_worker_compare_cards(
   NfcComparatorReaderWorkerCompareChecks* compare_checks,
   NfcDevice* card1,
   NfcDevice* card2,
   bool check_data) {
   furi_assert(compare_checks);
   furi_assert(card1);
   furi_assert(card2);

   // Compare protocols
   compare_checks->protocol = nfc_device_get_protocol(card1) == nfc_device_get_protocol(card2);

   // Get UIDs and lengths
   size_t uid_len1 = 0;
   const uint8_t* uid1 = nfc_device_get_uid(card1, &uid_len1);

   size_t uid_len2 = 0;
   const uint8_t* uid2 = nfc_device_get_uid(card2, &uid_len2);

   // Compare UID length
   compare_checks->uid_length = (uid_len1 == uid_len2);

   // Compare UID bytes if lengths match
   if(compare_checks->uid_length && uid1 && uid2) {
      compare_checks->uid = (memcmp(uid1, uid2, uid_len1) == 0);
   } else {
      compare_checks->uid = false;
   }

   // compare NFC data
   if(check_data) {
      compare_checks->nfc_data = nfc_device_is_equal(card1, card2);
   }
}
