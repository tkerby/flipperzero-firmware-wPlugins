#pragma once
#include <furi.h>
#include <cli/cli.h>
#include <storage/storage.h>
typedef struct {
    Storage* storage;
    FuriMutex* lock;
    unsigned state;
    int percent;
    char label[32];
} MegaBridge;
void mega_files_command(PipeSide* pipe, FuriString* args, void* context);
void mega_read_command(PipeSide* pipe, FuriString* args, void* context);
void mega_job_command(PipeSide* pipe, FuriString* args, void* context);
