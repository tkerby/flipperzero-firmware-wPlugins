#include "src/cuberzero.h"

typedef struct {
	uint32_t magicNumber;
} CBZSHEADER, *PCBZSHEADER;

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

	if(size < 4) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	CBZSHEADER header;

	if(storage_file_read(file, &header.magicNumber, 4) < 4) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	if(header.magicNumber != 0x43425A53) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	if(size < sizeof(CBZSHEADER)) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	storage_file_seek(file, 0, 1);

	if(storage_file_read(file, &header, sizeof(CBZSHEADER)) < sizeof(CBZSHEADER)) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	instance->session.file = file;
functionExit:
	storage_file_close(file);
	storage_file_free(file);
	furi_record_close(RECORD_STORAGE);
	return codeExit;
}

void SessionCleanup(const PCUBERZERO instance) {
	if(!instance->session.file) {
		return;
	}

	storage_file_close(instance->session.file);
	storage_file_free(instance->session.file);
}
