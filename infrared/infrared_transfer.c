#include <furi.h> // Explicitly include furi.h
#include <furi_hal.h> // Explicitly include furi_hal.h
#include "infrared_transfer.h"
#include <string.h>
#include <stdio.h>
#include <gui/view_dispatcher.h>
#include "../views/sending_view.h"
#include "../views/receiving_view.h"
#include <storage/storage.h> 
#include <notification/notification.h> 

#define CHUNK_SIZE 1
#define MAX_FILENAME_SIZE 32
#define MAX_BLOCK_SIZE 32
#define INIT_ADDR 0x10
#define NAME_ADDR 0xf0
#define REC_ADDR 0x1
#define CHECKSUM_ADDR 0x30

// IR
int32_t infrared_tx(void* context){

    App* app = (App*)context;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
	FURI_LOG_I("IRSender", "Starting thread, is_sending = %d", app->is_sending);
    FURI_LOG_E("IRSender", "going to send");
    bool redraw = true;
    with_view_model(
        app->send_view, SendingModel* _model, { furi_string_set(_model->status, "start sending meta"); furi_string_set(_model->progression, "started"); }, redraw);
    if (!storage_file_open(file, furi_string_get_cstr(app->send_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E("IRSender", "Fichier introuvable !");
        bool redraw = true;
        with_view_model(
            app->send_view, SendingModel* _model, { furi_string_set(_model->status, "error, file not found"); }, redraw);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
		app->is_sending = false;
        return 0;
    }

    float progress = 0.0f;
    app->is_sending = true;
    uint64_t bytes_sent = 0;
    uint64_t file_size = storage_file_size(file);

    if(file_size == 0) {
        FURI_LOG_E("IRSender", "Le fichier est vide !");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        bool redraw = true;
        with_view_model(
            app->send_view, SendingModel* _model, { furi_string_set(_model->status, "error, file not found"); }, redraw);
		app->is_sending = false;
        return 0;
    }

    const char* file_path = furi_string_get_cstr(app->send_path);
    const char* file_name = strrchr(file_path, '/'); 
    if(!file_name) file_name = file_path; 
    else file_name++; 

    uint8_t name_length = (uint8_t)strlen(file_name); 

    InfraredMessage message;
    message.protocol = infrared_get_protocol_by_name("NEC");
    message.address = NAME_ADDR;
    message.command = name_length;
    message.repeat = false;
    InfraredSignal* signal = infrared_signal_alloc();
    infrared_signal_set_message(signal, &message);
    if(infrared_signal_is_valid(signal)) {
        infrared_signal_transmit(signal);
    }
    infrared_signal_free(signal);

    for(uint8_t i = 0; i < name_length; i++) {
		if (!app->is_sending) {
			FURI_LOG_E("IRSender", "Thread stoppé");
			storage_file_close(file);
			storage_file_free(file);
			furi_record_close(RECORD_STORAGE);
			app->is_sending = false;
			return 0;
		} 
        message.address = NAME_ADDR;
        message.command = file_name[i];
        signal = infrared_signal_alloc();
        infrared_signal_set_message(signal, &message);
        if(infrared_signal_is_valid(signal)) {
            infrared_signal_transmit(signal);
        }
        infrared_signal_free(signal);

    }

    uint8_t size_bytes[4] = {
        (file_size >> 24) & 0xFF,
        (file_size >> 16) & 0xFF,
        (file_size >> 8)  & 0xFF,
        (file_size)       & 0xFF
    };

    for(int i = 0; i < 4; i++) {
		if (!app->is_sending) {
			FURI_LOG_E("IRSender", "Thread stoppé");
			storage_file_close(file);
			storage_file_free(file);
			furi_record_close(RECORD_STORAGE);
			app->is_sending = false;
			return 0;
		} 
        message.address = INIT_ADDR;
        message.command = size_bytes[i];
        signal = infrared_signal_alloc();
        infrared_signal_set_message(signal, &message);
        if(infrared_signal_is_valid(signal)) {
            infrared_signal_transmit(signal);
        }
        infrared_signal_free(signal);
    }

    uint8_t buffer[CHUNK_SIZE];
    uint8_t checksum = 0;
    size_t bytes_read;

    redraw = true;
    with_view_model(app->send_view, SendingModel* _model, {
        furi_string_set(_model->progression, "sending");
    }, redraw);

    while((bytes_read = storage_file_read(file, buffer, CHUNK_SIZE)) > 0) {
        
		if (!app->is_sending) {
			FURI_LOG_E("IRSender", "Thread stoppé");
			storage_file_close(file);
			storage_file_free(file);
			furi_record_close(RECORD_STORAGE);
			app->is_sending = false;
			return 0;
		} 

        checksum ^= buffer[0];
        message.address = REC_ADDR;
        message.command = buffer[0];
        signal = infrared_signal_alloc();
        infrared_signal_set_message(signal, &message);
        
		if(infrared_signal_is_valid(signal)) {
            infrared_signal_transmit(signal);
        }
        infrared_signal_free(signal);
        bytes_sent++;
        progress = (float)bytes_sent / (float)file_size;
        FURI_LOG_I("IRSender", "Progression : %.2f%%", (double)(progress * 100.0f));
        bool redraw = true;
        with_view_model(app->send_view, SendingModel* _model, {
            _model->loading = progress;
            furi_string_set(_model->status, "sending...");
        }, redraw);
        furi_delay_ms(5);
        FURI_LOG_I("IRSender", "Envoi octet : 0x%02X", buffer[0]);
    }

    message.address = CHECKSUM_ADDR;
    message.command = checksum;
    signal = infrared_signal_alloc();
    infrared_signal_set_message(signal, &message);
    if(infrared_signal_is_valid(signal)) {
        infrared_signal_transmit(signal);
    }
    infrared_signal_free(signal);
    FURI_LOG_I("IRSender", "Checksum envoyé : 0x%02X", checksum);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    app->is_sending = false;
	redraw = true;
	with_view_model(app->send_view, SendingModel* _model, {
		furi_string_set(_model->status, "File sent");
		_model->loading = 0;
		furi_string_set(_model->progression, "not started");
	}, redraw);

    return 0;
}



void infrared_rx_callback(void* context, InfraredWorkerSignal* received_signal) {
    
	InfraredController* controller = (InfraredController*)context;

	if(furi_mutex_acquire(controller->state_mutex, FuriWaitForever) != FuriStatusOk) {
        FURI_LOG_E("Infrared", "Échec de l'acquisition du mutex dans le callback");
        return;
	}
	
    const InfraredMessage* message = infrared_worker_get_decoded_signal(received_signal);
	FURI_LOG_I(TAG, "Received signal - signal address: %p", (void*)received_signal);
    if(message) {

		FURI_LOG_I(TAG, "Message reçu avec adresse : 0x%lx", (unsigned long)message->address);

		if(message->address == NAME_ADDR) {

			if (controller->is_receiving == true){
				Storage* storage = furi_record_open(RECORD_STORAGE);
				storage_common_remove(storage, FILE_PATH_RECV);
				furi_record_close(RECORD_STORAGE);
				controller->is_receiving = false;
			}

			bool redraw = true ;
			with_view_model(
                	controller->view, ReceivingModel* _model, { furi_string_set(_model->status, "gettin file name"); }, redraw);

			uint8_t received_char = (uint8_t)(message->command & 0xFF);

			FURI_LOG_I(TAG, "receving filename : %lx", (unsigned long)message->command);

			if(controller->file_name_index < (MAX_FILENAME_SIZE - 1)) {
				controller->file_name_buffer[controller->file_name_index] = received_char;
				controller->file_name_index++;

				FURI_LOG_I(TAG, "filename : %s", controller->file_name_buffer);

				if(received_char == '\0' || controller->file_name_index == (MAX_FILENAME_SIZE - 1)) {
					controller->file_name_buffer[controller->file_name_index] = '\0'; 
					FURI_LOG_I(TAG, "Nom de fichier reçu : %s", controller->file_name_buffer);
					controller->file_name_index = 0; 
        		}
    		} else {
				FURI_LOG_W(TAG, "Nom de fichier trop long, tronqué !");
				controller->file_name_buffer[MAX_FILENAME_SIZE - 1] = '\0';
				controller->file_name_index = 0;
			}
		}		

		if(message->address == INIT_ADDR){

			if (controller->is_receiving == true){
				Storage* storage = furi_record_open(RECORD_STORAGE);
				storage_common_remove(storage, FILE_PATH_RECV);
				furi_record_close(RECORD_STORAGE);
				controller->is_receiving = false;
			}

			bool redraw = true ;
			with_view_model(
                	controller->view, ReceivingModel* _model, { furi_string_set(_model->status, "gettin file size"); }, redraw);

			if(controller->size_file_count < 4) {
					controller->size_file_buffer[controller->size_file_count] = (uint8_t)(message->command & 0xFF);
					controller->size_file_count++;

					FURI_LOG_I(TAG, "Received byte %d for file size: 0x%02X", controller->size_file_count, (uint8_t)(message->command & 0xFF));

					if(controller->size_file_count == 4) {
						
						controller->file_size = (controller->size_file_buffer[0] << 24) |
									(controller->size_file_buffer[1] << 16) |
									(controller->size_file_buffer[2] << 8) |
									controller->size_file_buffer[3];

						FURI_LOG_I(TAG, "File size received: %lu bytes", controller->file_size);
						storage_file_open(controller->file, FILE_PATH_RECV, FSAM_WRITE, FSOM_CREATE_ALWAYS);

					with_view_model(
                	controller->view, ReceivingModel* _model, { furi_string_set(_model->progression, "started"); }, redraw);

						// Reset the counter for future use
						controller->is_receiving = true;
						controller->size_file_count =0;
						controller->bytes_received = 0; 
                		controller->progress = 0.0f;
					}
				} else {
					FURI_LOG_W(TAG, "Unexpected byte received for file size. Ignoring.");
				}

		}

		if(message->address == REC_ADDR) {

			if (controller->is_receiving == true){

				FURI_LOG_I(
				TAG,
				"Message reçu : protocole=%d, adresse=0x%lx, commande=0x%lx",
				message->protocol,
				(unsigned long)message->address,
				(unsigned long)message->command);

				uint8_t data[sizeof(message->command)];
				
				memcpy(data, &message->command, sizeof(message->command));
				if(controller->file) {

					storage_file_open(controller->file, FILE_PATH_RECV, FSAM_READ_WRITE, FSOM_OPEN_APPEND);

					uint8_t received_byte = (uint8_t)(message->command & 0xFF); 
					size_t bytes_written = storage_file_write(controller->file, &received_byte, 1);

					FURI_LOG_I(TAG, "Chunk reçu et écrit dans le fichier (%d octets)", sizeof(received_byte));
									
					storage_file_close(controller->file);

					FURI_LOG_I(TAG, "byte_written = %zu", bytes_written);

					if (bytes_written == 1){
						controller->bytes_received++; 
                    	controller->progress = (float)controller->bytes_received / (float)controller->file_size;

                        FURI_LOG_I(TAG, "Progression : %.2f%%", (double)(controller->progress * 100.0f));

						bool redraw = true ;
						with_view_model(
                			controller->view, ReceivingModel* _model, { _model->loading = controller->progress; furi_string_set(_model->status, "receiving...");}, redraw);
					} else {
                    FURI_LOG_E("IRReceiver", "Erreur lors de l'écriture du chunk");
                }

				} else {
					FURI_LOG_E(TAG, "Fichier non ouvert, impossible d'écrire les données");
				}

			}

        }

		else if (message->address == CHECKSUM_ADDR){

			FURI_LOG_I(TAG, "Receiving checksum");
			
			FURI_LOG_I(TAG, "Reçu checksum : 0x%lx", (unsigned long)message->command);

            uint8_t received_checksum = (uint8_t)(message->command & 0xFF);
            FURI_LOG_I(TAG, "Checksum reçu : 0x%02X", received_checksum);

			FURI_LOG_I(TAG, "Ouverture du fichier pour calculer le checksum...");
			storage_file_close(controller->file);
            uint8_t calculated_checksum = 0;
            storage_file_open(controller->file, FILE_PATH_RECV, FSAM_READ, FSOM_OPEN_EXISTING);

			size_t file_size = storage_file_size(controller->file);
			FURI_LOG_I(TAG, "Taille du fichier reçu : %zu octets", file_size);

            uint8_t buffer[1];
            while(storage_file_read(controller->file, buffer, 1) > 0) {
                calculated_checksum ^= buffer[0];
            }
            storage_file_close(controller->file);

            if(calculated_checksum == received_checksum) {
                FURI_LOG_I(TAG, "Checksum valide !");
				Storage* storage = furi_record_open(RECORD_STORAGE);
				controller->file_path = furi_string_alloc();
				furi_string_set(controller->file_path, FILE_PATH_FOLDER);
				furi_string_cat_str(controller->file_path, "/");
				furi_string_cat_str(controller->file_path, controller->file_name_buffer);

				FURI_LOG_I(TAG, "filename_path %s", furi_string_get_cstr(controller->file_path));

				uint8_t attempt = 0;
				while (storage_file_exists(storage, furi_string_get_cstr(controller->file_path))) {
					size_t dot_index = furi_string_search_rchar(controller->file_path, '.', 0);
					FuriString* base_name = furi_string_alloc();
					FuriString* extension = furi_string_alloc();
					
					if(dot_index != FURI_STRING_FAILURE) {

						furi_string_set_n(base_name, controller->file_path, 0, dot_index);
						furi_string_set(extension, controller->file_path);
						furi_string_mid(extension, dot_index, furi_string_size(controller->file_path) - dot_index);
					} else {
					
						furi_string_set(base_name, controller->file_path);
						furi_string_set(extension, ""); 
					}

					char suffix[4];
					snprintf(suffix, sizeof(suffix), "%02d", attempt + 1);

					FuriString* new_name = furi_string_alloc();
					furi_string_set(new_name, base_name);
					furi_string_cat_str(new_name, suffix);
					furi_string_cat(new_name, extension);
					furi_string_set(controller->file_path, furi_string_get_cstr(new_name));
					furi_string_free(base_name);
					furi_string_free(extension);
					furi_string_free(new_name);

					attempt++;
				}

				storage_common_copy(storage, FILE_PATH_RECV, furi_string_get_cstr(controller->file_path));
				storage_common_remove(storage, FILE_PATH_RECV);
				furi_record_close(RECORD_STORAGE);
				furi_string_free(controller->file_path);
				bool redraw = true;
				with_view_model(
					controller->view, ReceivingModel* _model, { 
						furi_string_printf(_model->status, "saved : %s", controller->file_name_buffer);
					}, 
					redraw
				);

            } else {
                FURI_LOG_E(TAG, "Checksum invalide : attendu 0x%02X, reçu 0x%02X", calculated_checksum, received_checksum);
				Storage* storage = furi_record_open(RECORD_STORAGE);
				storage_common_remove(storage, FILE_PATH_RECV);
				furi_record_close(RECORD_STORAGE);
				bool redraw = true ;
				with_view_model(
                	controller->view, ReceivingModel* _model, {furi_string_set(_model->status, "checksum error");}, redraw);
            }

			controller->is_receiving = false;

		}

    } else {
        FURI_LOG_W(TAG, "Message décodé est NULL");
    }

	FURI_LOG_I(TAG, "RX callback completed");
	furi_mutex_release(controller->state_mutex);

}



InfraredController* infrared_controller_alloc(void* context) {
    FURI_LOG_I(TAG, "Allocating InfraredController");

    InfraredController* controller = malloc(sizeof(InfraredController));
    if(!controller) {
        FURI_LOG_E(TAG, "Failed to allocate InfraredController");
        return NULL;
    }

	App* app = (App*)context;

    controller->worker = infrared_worker_alloc();
    controller->signal = infrared_signal_alloc();
    controller->processing_signal = false;
	controller->is_receiving = false;
	controller->size_file_count = 0;
	memset(controller->size_file_buffer, 0, sizeof(controller->size_file_buffer));
	controller->file_size = 0;
	controller->view = app->rec_view;  
	controller->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!controller->state_mutex) {
        FURI_LOG_E("Infrared", "Impossible d'initialiser le mutex");
        free(controller);
        return NULL;
    }
	
	Storage* storage = furi_record_open(RECORD_STORAGE);
    controller->file = storage_file_alloc(storage);

    if(controller->worker && controller->signal) {
        FURI_LOG_I(
            TAG, "InfraredWorker, InfraredSignal, and NotificationApp allocated successfully");
    } else {
        FURI_LOG_E(TAG, "Failed to allocate resources");
        free(controller);
        return NULL;
    }

    FURI_LOG_I(TAG, "Setting up RX callback");
    infrared_worker_rx_set_received_signal_callback(
        controller->worker, infrared_rx_callback, controller);

    FURI_LOG_I(TAG, "InfraredController allocated successfully");

	infrared_worker_rx_start(controller->worker);

    return controller;
}

void infrared_controller_free(InfraredController* controller) {
    FURI_LOG_I(TAG, "Freeing InfraredController");

    if(controller) {
        if(controller->worker_rx_active) {
            FURI_LOG_I(TAG, "Stopping RX worker");
            infrared_worker_rx_stop(controller->worker);
        }

		controller->size_file_count = 0;

		infrared_worker_rx_stop(controller->worker);
    	infrared_worker_rx_set_received_signal_callback(controller->worker, NULL, NULL);

        FURI_LOG_I(TAG, "Freeing InfraredWorker and InfraredSignal");
        infrared_worker_free(controller->worker);
        infrared_signal_free(controller->signal);

        FURI_LOG_I(TAG, "Closing NotificationApp");
        furi_record_close(RECORD_NOTIFICATION);

		if(controller->file) {
            storage_file_close(controller->file);
            storage_file_free(controller->file);
        }

        furi_record_close(RECORD_STORAGE);

		if (controller->thread){
			
			furi_thread_free(controller->thread);
			controller->thread = NULL;
		}

		furi_mutex_free(controller->state_mutex);
        free(controller);
		
        FURI_LOG_I(TAG, "InfraredController freed successfully");
    } else {
        FURI_LOG_W(TAG, "Attempted to free NULL InfraredController");
    }
}