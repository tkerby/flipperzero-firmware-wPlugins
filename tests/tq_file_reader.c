#include "tq_file_reader.h"

#include <stdio.h>
#include <stdlib.h>

static bool file_read_at(void* ctx, uint32_t offset, void* buf, size_t len) {
    FILE* f = (FILE*)ctx;
    if(fseek(f, (long)offset, SEEK_SET) != 0) return false;
    return fread(buf, 1, len, f) == len;
}

bool tq_test_reader_open(Tq_Reader* reader, const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    reader->read_at = file_read_at;
    reader->ctx = f;
    reader->size = (uint32_t)size;
    return true;
}

void tq_test_reader_close(Tq_Reader* reader) {
    if(reader->ctx) fclose((FILE*)reader->ctx);
    reader->ctx = NULL;
}
