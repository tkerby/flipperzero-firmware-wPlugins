#include "cuberzero.h"
#include <input/input.h>
#include <furi_hal_light.h>

#define LIGHT(red, green, blue)                \
	do {                                       \
		furi_hal_light_set(LightRed, red);     \
		furi_hal_light_set(LightGreen, green); \
		furi_hal_light_set(LightBlue, blue);   \
	} while(0)

int32_t cuberzeroMain(const void* const pointer) {
	UNUSED(pointer);
	CUBERZERO_LOG("Initializing");
	FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
	furi_check(queue, "furi_message_queue_alloc() failed");
	InputEvent event;

	while(1) {
		furi_check(furi_message_queue_get(queue, &event, FuriWaitForever) == FuriStatusOk, "furi_message_queue_get() failed");

		if(event.key == InputKeyOk) {
			if(event.type == InputTypePress) {
				LIGHT(255, 0, 0);
			} else if(event.type == InputTypeRelease) {
				LIGHT(0, 255, 0);
			}
		}

		if(event.key == InputKeyBack) {
			LIGHT(0, 0, 0);
			break;
		}
	}

	furi_message_queue_free(queue);
	CUBERZERO_LOG("Exiting");
	return 0;
}
