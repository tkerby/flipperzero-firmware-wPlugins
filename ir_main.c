#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>
#include <ir_transfer_icons.h>
#include <stdlib.h>
#include <infrared.h>
#include "infrared_signal.h"
#include <infrared_worker.h>

#define BACKLIGHT_ON 1
#define TEXT_VIEWER_EXTENSION "*"
#define INIT_ADDR 0x10
#define NAME_ADDR 0xf0
#define REC_ADDR 0x1
#define CHECKSUM_ADDR 0x30
#define CHUNK_SIZE 1
#define MAX_FILENAME_SIZE 32
#define MAX_BLOCK_SIZE 32
#define FILE_PATH_RECV "/ext/tmpfile"
#define FILE_PATH_FOLDER "/ext/downloads"
#define TAG "IR Transfer"
#define THREAD_SIGNAL_DONE 0x01
#define TIMEOUT_MS 5000

typedef struct InfraredController {
	bool is_receiving;
    InfraredWorker* worker;
    bool worker_rx_active;
    InfraredSignal* signal;
    bool processing_signal;
	FuriThread* thread;
	File* file;
	FuriMutex* state_mutex;
	int size_file_count;
	uint8_t size_file_buffer[4];
	uint32_t file_size;
	View* view;
	uint32_t bytes_received;
	float progress;
	char file_name_buffer[MAX_FILENAME_SIZE];
	uint8_t file_name_index;
	FuriString* file_path;

} InfraredController;

typedef enum {
    LandingView, 
    RecView,
	SendView,
	TextInputView 
} AppViews;


typedef enum {
    RedrawSendScreen = 0, 
} EventId;

typedef struct {
    ViewDispatcher* view_dispatcher; 
    TextInput* text_input;
    View* landing_view;
	View* rec_view;
	View* send_view; 
    char* temp_buffer; 
    uint32_t temp_buffer_size; 
	FuriString* send_path;
	FuriString* rec_path;
	InfraredController* send_ir_controller;
	InfraredController* rec_ir_controller;
	FuriThread* send_thread;
	volatile bool is_sending;
} App;

typedef struct {
	char* landing;
} LandingModel;

typedef struct {
	char* isReceving;
	float loading;
	FuriString* receiving_path; 
	FuriString* status;
	FuriString* progression;
} ReceivingModel;

typedef struct {
	bool isSending;
	float loading;
	FuriString* sending_path; 
	FuriString* status;
	FuriString* progression;
} SendingModel;

// IR
static int32_t infrared_tx(void* context){

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



static void infrared_rx_callback(void* context, InfraredWorkerSignal* received_signal) {
    
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

		else if (message->address != REC_ADDR && message->address != INIT_ADDR &&message->address != NAME_ADDR){

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

static uint32_t landing_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}


static uint32_t receiving_exit_callback(void* _context) {
    UNUSED(_context);
    return LandingView;
}

static uint32_t sending_exit_callback(void* _context) {
    UNUSED(_context);
    return LandingView;
}

void draw_truncated_text(Canvas* canvas, int x, int y, const char* text, int max_width) {
    int text_width = canvas_string_width(canvas, text);
    
    if(text_width <= max_width) {

        canvas_draw_str(canvas, x, y, text);
        return;
    }

    const char* ellipsis = "...";
    size_t ellipsis_width = canvas_string_width(canvas, ellipsis);
    size_t available_width = max_width - ellipsis_width;
    size_t text_len = strlen(text);
    size_t start_len = 0;
    size_t end_len = 0;
    int start_width = 0;
    int end_width = 0;

	while(start_len < text_len && start_width + canvas_glyph_width(canvas, text[start_len]) <= (size_t)(available_width / 2) + 1) {
        start_width += canvas_glyph_width(canvas, text[start_len]);
        start_len++;
    }

	while(end_len < text_len - start_len && end_width + canvas_glyph_width(canvas, text[text_len - 1 - end_len]) <= (size_t)(available_width / 2) + 1) {
        end_width += canvas_glyph_width(canvas, text[text_len - 1 - end_len]);
        end_len++;
    }

    char truncated_text[128]; 
    snprintf(truncated_text, sizeof(truncated_text), "%.*s%s%.*s", (int)start_len, text, ellipsis, (int)end_len, text + (text_len - end_len));
    canvas_draw_str(canvas, x, y, truncated_text);
}



static void Landing_draw_callback(Canvas* canvas, void* model) {
	UNUSED(model);
	static const uint8_t image_IR_2_0_bits[] = {0x1c,0xff,0x01,0xfc,0x77,0x1c,0xe0,0x7f,0x1c,0xff,0x03,0xfc,0x77,0x1c,0xe0,0x3f,0x1c,0x83,0x03,0x0c,0x70,0x0c,0x60,0x00,0x8c,0x83,0x03,0x0e,0x30,0x0e,0x70,0x00,0x8c,0x83,0x03,0x0e,0x30,0x0e,0x70,0x00,0x8e,0xc3,0x01,0xfe,0x39,0x0e,0xf0,0x1f,0x8e,0xff,0x00,0xfe,0x39,0x0e,0xf0,0x1f,0x8e,0xff,0x00,0x07,0x38,0x06,0x30,0x00,0xc6,0xc1,0x01,0x07,0x18,0x07,0x38,0x00,0xc6,0xc1,0x01,0x07,0x18,0x07,0x38,0x00,0xc7,0xe1,0x00,0x07,0x1c,0x07,0x38,0x00,0xc7,0xe0,0x00,0x03,0x1c,0xff,0xf9,0x1f,0xc7,0xe0,0x81,0x03,0x1c,0xff,0xf8,0x0f};
	static const uint8_t image_TR_1_0_bits[] = {0xff,0xf3,0x1f,0xf0,0xc0,0xc1,0xe1,0x87,0xff,0xfc,0xcf,0x7f,0xff,0xf3,0x3f,0xf0,0xc0,0xc1,0xf8,0x9f,0xff,0xfc,0xc7,0xff,0x38,0x30,0x38,0xf8,0xe0,0xc3,0x38,0x9c,0x01,0x0c,0xc0,0xe0,0x38,0x38,0x38,0xf8,0xe0,0xe3,0x1c,0xdc,0x01,0x0e,0xe0,0xe0,0x38,0x38,0x38,0xec,0xe1,0xe3,0x1c,0xc0,0x01,0x0e,0xe0,0xe0,0x38,0x38,0x1c,0xcc,0x61,0x67,0xfc,0xc0,0x3f,0xfe,0xe3,0x70,0x18,0xf8,0x0f,0xc6,0x61,0x66,0xf8,0xc7,0x3f,0xfe,0xe3,0x3f,0x1c,0xf8,0x0f,0xc7,0x71,0x6e,0xc0,0xef,0x00,0x06,0xe0,0x3f,0x1c,0x1c,0x1c,0xff,0x71,0x7c,0x00,0xee,0x00,0x07,0x70,0x70,0x1c,0x1c,0x9c,0xff,0x71,0x7c,0x07,0xee,0x00,0x07,0x70,0x70,0x0c,0x1c,0x8e,0xc1,0x33,0x3c,0x0f,0xe7,0x00,0x07,0x70,0x38,0x0c,0x0c,0xce,0x81,0x33,0x38,0xfe,0x67,0x00,0xff,0x33,0x38,0x0e,0x0c,0xde,0x80,0x3b,0x38,0xfc,0x71,0x00,0xff,0x31,0x78};
	static const uint8_t image_folder_open_0_bits[] = {0x00,0xc0,0x02,0x00,0x20,0x03,0x00,0x90,0x03,0x3e,0x00,0x00,0xc1,0x3f,0x00,0x01,0x40,0x00,0x01,0x40,0x00,0x01,0x40,0x00,0xe1,0xff,0x03,0x11,0x00,0x02,0x11,0x00,0x02,0x09,0x00,0x01,0x09,0x00,0x01,0x05,0x80,0x00,0x05,0x80,0x00,0xfe,0x7f,0x00};
	canvas_set_bitmap_mode(canvas, true);
	canvas_draw_xbm(canvas, 20, 9, 63, 13, image_IR_2_0_bits);
	canvas_draw_xbm(canvas, 14, 29, 96, 13, image_TR_1_0_bits);
	canvas_draw_xbm(canvas, 89, 6, 18, 16, image_folder_open_0_bits);
	canvas_set_font(canvas, FontPrimary);
	elements_button_left(canvas, "Send");
	elements_button_right(canvas, "Receive");


}

static void Receiving_draw_callback(Canvas* canvas, void* model) {
    ReceivingModel* rec_model = (ReceivingModel*)model;
	canvas_clear(canvas);
	canvas_set_bitmap_mode(canvas, true);
	canvas_set_font(canvas, FontPrimary);
	canvas_draw_str(canvas, 0, 12, "IR FILE RECEIVER");
	canvas_set_font(canvas, FontSecondary);
	canvas_draw_str(canvas, 0, 23, "path :");
	canvas_draw_str(canvas, 28, 23, furi_string_get_cstr(rec_model->receiving_path));
	canvas_draw_str(canvas, 0, 34, "status :");
	draw_truncated_text(canvas, 35, 34, furi_string_get_cstr(rec_model->status), 98);
	canvas_draw_str(canvas, 0, 44, "progress :");
	if(furi_string_cmp_str(rec_model->progression, "not started") == 0) {
		canvas_draw_str(canvas, 45, 44, furi_string_get_cstr(rec_model->progression));	
	} else {
		elements_progress_bar(canvas, 46, 37, 75, rec_model->loading);
	}	
	elements_button_center(canvas, "reset");
}

static void Sending_draw_callback(Canvas* canvas, void* model) {
    
	SendingModel* send_model = (SendingModel*)model;
	canvas_clear(canvas);
	canvas_set_bitmap_mode(canvas, true);
	canvas_set_font(canvas, FontPrimary);
	canvas_draw_str(canvas, 0, 12, "IR FILE SENDER");
	canvas_set_font(canvas, FontSecondary);
	canvas_draw_str(canvas, 0, 23, "file :");
	draw_truncated_text(canvas, 18, 23, furi_string_get_cstr(send_model->sending_path), 100);
	canvas_draw_str(canvas, 0, 34, "status :");
	draw_truncated_text(canvas, 35, 34, furi_string_get_cstr(send_model->status), 100);
	canvas_draw_str(canvas, 0, 44, "progress :");
	if(furi_string_cmp_str(send_model->progression, "not started") == 0) {
		canvas_draw_str(canvas, 45, 44, furi_string_get_cstr(send_model->progression));
		elements_button_right(canvas, "Send");
		elements_button_left(canvas, "change file");
		
	} else {
		elements_progress_bar(canvas, 46, 37, 75, send_model->loading);
		elements_button_center(canvas, "Cancel");
	}
	
}

static void send_view_enter_callback(void* context) {

    App* app = (App*)context;
	FURI_LOG_I("Send_view", "entering send view before browser");

	if(furi_string_cmp_str(app->send_path, "none") == 0) {
        
            furi_string_set(app->send_path, "/ext");
            DialogsFileBrowserOptions browser_options;
			dialog_file_browser_set_basic_options(
            &browser_options, TEXT_VIEWER_EXTENSION, NULL);
            browser_options.hide_ext = false;
            DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
            dialog_file_browser_show(dialogs, app->send_path, app->send_path, &browser_options); // &browser_options
            furi_record_close(RECORD_DIALOGS);
			bool redraw = true ;
			with_view_model(
                app->send_view, SendingModel* _model, { furi_string_set(_model->sending_path, app->send_path); _model->loading = 0;													
				furi_string_set(_model->status, "ready to send"); furi_string_set(_model->progression, "not started"); }, redraw);			
	}

}

static void send_view_exit_callback(void* context) {
    App* app = (App*)context;
	furi_string_set(app->send_path, "none");
	app->is_sending = false;
	if(app->send_thread) {
		if(furi_thread_get_state(app->send_thread) != FuriThreadStateStopped) {
        furi_thread_join(app->send_thread); 
    }
		furi_thread_free(app->send_thread); 
		app->send_thread = NULL;
	}
}


static bool send_view_custom_event_callback(uint32_t event, void* context) {
    App* app = (App*)context;
    switch(event) {
    case RedrawSendScreen:
        {
            bool redraw = true;
            with_view_model(
                app->send_view, SendingModel* _model, { UNUSED(_model); }, redraw);
            return true;
        }
 
    default:
        return false;
    }
}


static void rec_view_enter_callback(void* context) {

    App* app = (App*)context;

	FURI_LOG_I("rec_view_enter", "about to allocate infrared controller");

	bool redraw = true ;
	with_view_model(
                app->rec_view, ReceivingModel* _model, 
				{ _model->loading = 0;													
				furi_string_set(_model->status, "awaiting signal");
				furi_string_set(_model->receiving_path, FILE_PATH_FOLDER);
				furi_string_set(_model->progression, "not started");
				 }, redraw);			
	
	app->rec_ir_controller = infrared_controller_alloc(app);


}

static void rec_view_exit_callback(void* context) {
    App* app = (App*)context;
	infrared_controller_free(app->rec_ir_controller);

}

static bool Landing_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {

			FURI_LOG_I("Send", "Switching to SendView");
			view_dispatcher_switch_to_view(app->view_dispatcher, SendView);
			return true;
  
        } else if(event->key == InputKeyRight) {
			FURI_LOG_I("Rec", "Switching to RecView");
			view_dispatcher_switch_to_view(app->view_dispatcher, RecView);

			return true;
        }
    } 
    return false;
}


static bool Sending_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;

	if(app->send_thread && furi_thread_get_state(app->send_thread) == FuriThreadStateStopped) {
        furi_thread_free(app->send_thread);
        app->send_thread = NULL;
        FURI_LOG_I("IRSender", "Thread terminé détecté, libéré proprement");
    }

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            FURI_LOG_I("Send", "IR sending function");

            if (app->is_sending) {
                FURI_LOG_I("IRSender", "Demande d'arrêt du thread");
                app->is_sending = false; 

                furi_delay_ms(10);

                if(app->send_thread) {

					if(furi_thread_get_state(app->send_thread) != FuriThreadStateStopped) {
					furi_thread_join(app->send_thread); 
					furi_thread_free(app->send_thread); 
					app->send_thread = NULL;
					}
                    }
					with_view_model(
                		app->send_view, SendingModel* _model, {
                    	_model->loading = 0;
                    	furi_string_set(_model->status, "ready to send");
                    	furi_string_set(_model->progression, "not started");
                	}, true);

				return true;

            }

        } 
        else if(event->key == InputKeyRight) {
            if (!app->is_sending) {
                if(!app->send_thread) {
                    FURI_LOG_I("IRSender", "Lancement du thread IRSendThread");
                    app->send_thread = furi_thread_alloc_ex(
                        "IRSendThread",
                        1024,
                        infrared_tx,
                        app
                    );
                    furi_thread_start(app->send_thread);
                } else {
                    FURI_LOG_E("IRSender", "Le thread est déjà actif !");
                }
            }
            return true;
        } 
        else if(event->key == InputKeyLeft) {
            if (!app->is_sending) {
                DialogsFileBrowserOptions browser_options;
                dialog_file_browser_set_basic_options(
                    &browser_options, TEXT_VIEWER_EXTENSION, NULL);
                browser_options.hide_ext = false;
                DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
                dialog_file_browser_show(dialogs, app->send_path, app->send_path, &browser_options);
                furi_record_close(RECORD_DIALOGS);

                bool redraw = true;
                with_view_model(
                    app->send_view, SendingModel* _model, {
                        furi_string_set(_model->sending_path, app->send_path);
                    }, redraw);
            }
            return true;
        }
    }

    return false; // << ICI tu dois retourner false si aucun bouton n'a matché
}





static bool Receiving_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {

			return true;
  
        } else if(event->key == InputKeyOk) {
			view_dispatcher_switch_to_view(app->view_dispatcher, RecView);

			return true;
        }
    } 
    return false;
}

static App* app_alloc() {
    App* app = (App*)malloc(sizeof(App));
    Gui* gui = furi_record_open(RECORD_GUI);

	app->send_path = furi_string_alloc();

	Storage* storage = furi_record_open(RECORD_STORAGE);
	if (!storage_dir_exists(storage, FILE_PATH_FOLDER)){
		if(storage_simply_mkdir(storage, FILE_PATH_FOLDER)){
			FURI_LOG_I("ALLOC", "Download_folder OK");
		} else {
			FURI_LOG_E("ALLOC", "Download_folder NOT OK");
		}
	}
	furi_record_close(RECORD_STORAGE);
	
	furi_string_set(app->send_path, "none");

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TextInputView, text_input_get_view(app->text_input));
    app->temp_buffer_size = 32;
    app->temp_buffer = (char*)malloc(app->temp_buffer_size);

    app->landing_view = view_alloc();
    view_set_draw_callback(app->landing_view, Landing_draw_callback);
    view_set_input_callback(app->landing_view, Landing_input_callback);
    view_set_previous_callback(app->landing_view, landing_exit_callback);
	view_set_context(app->landing_view, app);
    view_allocate_model(app->landing_view, ViewModelTypeLockFree, sizeof(LandingModel));
    LandingModel* landing_model = view_get_model(app->landing_view);
	UNUSED(landing_model);
    view_dispatcher_add_view(app->view_dispatcher, LandingView, app->landing_view);


	app->rec_view = view_alloc();
    view_set_draw_callback(app->rec_view, Receiving_draw_callback);
    view_set_input_callback(app->rec_view, Receiving_input_callback);
    view_set_previous_callback(app->rec_view, receiving_exit_callback);
	view_set_context(app->rec_view, app);
    view_set_enter_callback(app->rec_view, rec_view_enter_callback);
    view_set_exit_callback(app->rec_view, rec_view_exit_callback);
    view_allocate_model(app->rec_view, ViewModelTypeLockFree, sizeof(ReceivingModel));
    ReceivingModel* receving_model = view_get_model(app->rec_view);
    receving_model->status =furi_string_alloc();
	receving_model->receiving_path = furi_string_alloc();
	receving_model->progression = furi_string_alloc();

    view_dispatcher_add_view(app->view_dispatcher, RecView, app->rec_view);

	app->send_view = view_alloc();
    view_set_draw_callback(app->send_view, Sending_draw_callback);
    view_set_input_callback(app->send_view, Sending_input_callback);
    view_set_previous_callback(app->send_view, sending_exit_callback);
	view_set_context(app->send_view, app);
    view_set_enter_callback(app->send_view, send_view_enter_callback);
    view_set_exit_callback(app->send_view, send_view_exit_callback);
    view_set_custom_callback(app->send_view, send_view_custom_event_callback);
    view_allocate_model(app->send_view, ViewModelTypeLockFree, sizeof(SendingModel));
    SendingModel* sending_model = view_get_model(app->send_view);
	sending_model->status = furi_string_alloc();
	sending_model->sending_path = furi_string_alloc();
	sending_model->progression = furi_string_alloc();
	furi_string_set(sending_model->sending_path, "none");
    view_dispatcher_add_view(app->view_dispatcher, SendView, app->send_view);


    app->notifications = furi_record_open(RECORD_NOTIFICATION);

	view_dispatcher_switch_to_view(app->view_dispatcher, LandingView);

    return app;
}


static void app_free(App* app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);

	furi_string_free(app->send_path);
	SendingModel* sending_model = view_get_model(app->send_view);
	furi_string_free(sending_model->sending_path);
	furi_string_free(sending_model->status);
	furi_string_free(sending_model->progression);

	ReceivingModel* receving_model = view_get_model(app->rec_view);
	furi_string_free(receving_model->status);
	furi_string_free(receving_model->receiving_path);
	furi_string_free(receving_model->progression);

    view_dispatcher_remove_view(app->view_dispatcher, TextInputView);
    text_input_free(app->text_input);
    free(app->temp_buffer);
    view_dispatcher_remove_view(app->view_dispatcher, LandingView);
    view_free(app->landing_view);
	view_dispatcher_remove_view(app->view_dispatcher, RecView);
    view_free(app->rec_view);
	view_dispatcher_remove_view(app->view_dispatcher, SendView);
    view_free(app->send_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}


int32_t main_app(void* _p) {
    UNUSED(_p);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}