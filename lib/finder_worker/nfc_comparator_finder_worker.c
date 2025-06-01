#include "nfc_comparator_finder_worker.h"

static void nfc_comparator_finder_worker_scanner_callback(NfcScannerEvent event, void* context) {
   furi_assert(context);
   NfcComparatorFinderWorker* nfc_comparator_finder_worker = context;
   switch(event.type) {
   case NfcScannerEventTypeDetected:
      nfc_comparator_finder_worker->protocol = event.data.protocols;
      nfc_comparator_finder_worker->state = NfcComparatorFinderWorkerState_Polling;
      break;
   default:
      break;
   }
}

static NfcCommand
   nfc_comparator_finder_worker_poller_callback(NfcGenericEvent event, void* context) {
   UNUSED(event);
   furi_assert(context);
   NfcComparatorFinderWorker* nfc_comparator_finder_worker = context;
   nfc_comparator_finder_worker->state = NfcComparatorFinderWorkerState_Finding;
   return NfcCommandStop;
}

static int32_t nfc_comparator_finder_worker_task(void* context) {
   NfcComparatorFinderWorker* worker = context;
   furi_assert(worker);
   while(worker->state != NfcComparatorFinderWorkerState_Stopped) {
      switch(worker->state) {
      case NfcComparatorFinderWorkerState_Scanning: {
         NfcScanner* nfc_scanner = nfc_scanner_alloc(worker->nfc);
         nfc_scanner_start(nfc_scanner, nfc_comparator_finder_worker_scanner_callback, worker);
         while(worker->state == NfcComparatorFinderWorkerState_Scanning) {
            furi_delay_ms(100);
         }
         nfc_scanner_stop(nfc_scanner);
         break;
      }
      case NfcComparatorFinderWorkerState_Polling: {
         NfcPoller* nfc_poller = nfc_poller_alloc(worker->nfc, worker->protocol[0]);
         nfc_poller_start(nfc_poller, nfc_comparator_finder_worker_poller_callback, worker);
         while(worker->state == NfcComparatorFinderWorkerState_Polling) {
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
      case NfcComparatorFinderWorkerState_Finding: {
         if(dir_walk_open(worker->dir_walk, "/ext/nfc")) {
            FuriString* ext = furi_string_alloc();
            while(dir_walk_read(worker->dir_walk, worker->compare_checks->nfc_card_path, NULL) ==
                  DirWalkOK) {
               path_extract_ext_str(worker->compare_checks->nfc_card_path, ext);

               if(furi_string_cmpi_str(ext, ".nfc") == 0) {
                  if(nfc_device_load(
                        worker->loaded_nfc_card,
                        furi_string_get_cstr(worker->compare_checks->nfc_card_path))) {
                     nfc_comparator_finder_worker_compare_cards(
                        worker->compare_checks,
                        worker->scanned_nfc_card,
                        worker->loaded_nfc_card,
                        false);

                     if(worker->compare_checks->uid && worker->compare_checks->uid_length &&
                        worker->compare_checks->protocol) {
                        break;
                     }
                  }
               }
            }

            dir_walk_close(worker->dir_walk);
            furi_string_free(ext);
            nfc_device_free(worker->scanned_nfc_card);
            worker->scanned_nfc_card = NULL;
            nfc_device_free(worker->loaded_nfc_card);
            worker->loaded_nfc_card = NULL;

            worker->state = NfcComparatorFinderWorkerState_Stopped;
            break;
         }
      }
      default:
         break;
      }
   }
   return 0;
}

NfcComparatorFinderWorker*
   nfc_comparator_finder_worker_alloc(NfcComparatorFinderWorkerCompareChecks* compare_checks) {
   NfcComparatorFinderWorker* worker = calloc(1, sizeof(NfcComparatorFinderWorker));
   if(!worker) return NULL;
   worker->nfc = nfc_alloc();
   if(!worker->nfc) {
      free(worker);
      return NULL;
   }

   worker->compare_checks = compare_checks;

   worker->thread = furi_thread_alloc();
   furi_thread_set_name(worker->thread, "NfcComparatorFinderWorker");
   furi_thread_set_context(worker->thread, worker);
   furi_thread_set_stack_size(worker->thread, 1024);
   furi_thread_set_callback(worker->thread, nfc_comparator_finder_worker_task);

   if(!worker->thread) {
      nfc_free(worker->nfc);
      free(worker);
      return NULL;
   }
   worker->state = NfcComparatorFinderWorkerState_Stopped;
   worker->scanned_nfc_card = NULL;
   worker->loaded_nfc_card = nfc_device_alloc();
   worker->protocol = NULL;

   worker->dir_walk = dir_walk_alloc(furi_record_open(RECORD_STORAGE));
   return worker;
}

void nfc_comparator_finder_worker_free(NfcComparatorFinderWorker* worker) {
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
   if(worker->dir_walk) {
      dir_walk_free(worker->dir_walk);
      worker->dir_walk = NULL;
   }
   free(worker);
}

void nfc_comparator_finder_worker_stop(NfcComparatorFinderWorker* worker) {
   furi_assert(worker);
   if(worker->state != NfcComparatorFinderWorkerState_Stopped) {
      worker->state = NfcComparatorFinderWorkerState_Stopped;
      furi_thread_join(worker->thread);
   }
}

void nfc_comparator_finder_worker_start(NfcComparatorFinderWorker* worker) {
   furi_assert(worker);
   if(worker->state == NfcComparatorFinderWorkerState_Stopped) {
      worker->state = NfcComparatorFinderWorkerState_Scanning;
      furi_thread_start(worker->thread);
   }
}

NfcComparatorFinderWorkerState
   nfc_comparator_finder_worker_get_state(const NfcComparatorFinderWorker* worker) {
   furi_assert(worker);
   return worker->state;
}

void nfc_comparator_finder_worker_compare_cards(
   NfcComparatorFinderWorkerCompareChecks* compare_checks,
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
