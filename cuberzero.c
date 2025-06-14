#include "cuberzero.h"
#include <furi_hal_light.h>

int32_t cuberzeroMain(const void* const pointer) {
	UNUSED(pointer);
	CUBERZERO_LOG("Initializing");
	furi_hal_light_set(LightBlue, 255);
	return 0;
}
