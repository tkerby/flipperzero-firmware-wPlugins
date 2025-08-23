#include "src/cuberzero.h"

uint8_t SessionOpen(const PCUBERZERO instance, const char* const path) {
	UNUSED(instance);
	UNUSED(path);
	Storage* storage = furi_record_open(RECORD_STORAGE);
	File* file = storage_file_alloc(storage);
	uint8_t codeExit = SESSION_SUCCESS;

	if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
		codeExit = SESSION_OPEN_FAILED;
		goto functionExit;
	}

	uint64_t size = storage_file_size(file);
functionExit:
	storage_file_close(file);
	storage_file_free(file);
	furi_record_close(RECORD_STORAGE);
	return codeExit;
}
