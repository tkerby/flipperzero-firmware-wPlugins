"""Run the actual firmware callbacks with bounded storage/pipe stubs."""

from pathlib import Path
import subprocess
import tempfile

source = (
    (Path(__file__).resolve().parents[1] / "bridge.c")
    .read_text()
    .replace('#include "bridge.h"', "")
)
header = r"""
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define FuriWaitForever 0
#define FSF_DIRECTORY 1
#define FSAM_READ 0
#define FSOM_OPEN_EXISTING 0
typedef void Storage;
typedef void FuriMutex;
typedef struct { const char* text; } FuriString;
typedef struct { char response[1024]; size_t available; } PipeSide;
typedef struct { size_t offset; bool listed; } File;
typedef struct { unsigned flags; uint64_t size; } FileInfo;
typedef struct { Storage* storage; FuriMutex* lock; unsigned state; int percent; char label[32]; } MegaBridge;
static unsigned char capture[300];
static unsigned opened=0;
static uint32_t ticks=0;
static uint32_t furi_get_tick(void) {return ticks;}
static uint32_t furi_ms_to_ticks(uint32_t ms) {return ms;}
static void furi_delay_ms(uint32_t ms) {ticks+=ms;}
static const char* furi_string_get_cstr(FuriString* args) { return args->text; }
static size_t pipe_spaces_available(PipeSide* pipe) {return pipe->available;}
static size_t pipe_send(PipeSide* pipe,const void* data,size_t n) {size_t used=strlen(pipe->response);assert(used+n<sizeof(pipe->response));memcpy(pipe->response+used,data,n);pipe->response[used+n]=0;return n;}
static void furi_mutex_acquire(void* lock,int time) {(void)lock;(void)time;}
static void furi_mutex_release(void* lock) {(void)lock;}
static File* storage_file_alloc(Storage* storage) {(void)storage;return calloc(1,sizeof(File));}
static void storage_file_free(File* file) {free(file);}
static bool storage_dir_open(File* file,const char* path) {(void)file;return strcmp(path,"/ext/apps_data/marauder/pcaps")==0;}
static bool storage_dir_read(File* file,FileInfo* info,char* name,uint16_t size) {
 if(file->listed)return false;file->listed=true;info->size=sizeof(capture);info->flags=0;snprintf(name,size,"saved.pcap");return true;
}
static void storage_dir_close(File* file) {(void)file;}
static bool storage_file_open(File* file,const char* path,int access,int mode) {(void)file;(void)access;(void)mode;opened++;return strcmp(path,"/ext/apps_data/marauder/pcaps/saved.pcap")==0;}
static uint64_t storage_file_size(File* file) {(void)file;return sizeof(capture);}
static bool storage_file_seek(File* file,uint32_t offset,bool start) {(void)start;file->offset=offset;return offset<=sizeof(capture);}
static size_t storage_file_read(File* file,void* data,size_t n) {assert(file->offset+n<=sizeof(capture));memcpy(data,capture+file->offset,n);file->offset+=n;return n;}
static void storage_file_close(File* file) {(void)file;}
"""
tests = r"""
int main(void) {
 MegaBridge bridge={0};PipeSide pipe={.available=192};FuriString args={"1 0"};
 for(unsigned i=0;i<sizeof(capture);i++)capture[i]=(unsigned char)i;
 mega_files_command(&pipe,&args,&bridge);assert(strstr(pipe.response,"FILE 300 73617665642e70636170"));
 pipe.response[0]=0;args.text="2 1";mega_files_command(&pipe,&args,&bridge);assert(strstr(pipe.response,"2 END"));
 pipe.response[0]=0;args.text="3 73617665642e70636170 0";mega_read_command(&pipe,&args,&bridge);
 assert(strstr(pipe.response,"3 DATA 300 0 256 29058c73 00010203"));
 pipe.response[0]=0;args.text="4 73617665642e70636170 256";mega_read_command(&pipe,&args,&bridge);assert(strstr(pipe.response,"4 DATA 300 256 44"));
 pipe.response[0]=0;args.text="5 73617665642e70636170 301";mega_read_command(&pipe,&args,&bridge);assert(strstr(pipe.response,"ERROR size-or-offset"));
 unsigned before=opened;
 pipe.response[0]=0;args.text="6 2e2e2f73617665642e70636170 0";mega_read_command(&pipe,&args,&bridge);assert(strstr(pipe.response,"ERROR invalid-file"));assert(opened==before);
 pipe.response[0]=0;args.text="7 0073617665642e70636170 0";mega_read_command(&pipe,&args,&bridge);assert(opened==before);
 pipe.response[0]=0;args.text="8 1 50 Hashcat";mega_job_command(&pipe,&args,&bridge);assert(bridge.state==1&&bridge.percent==50&&!strcmp(bridge.label,"Hashcat"));
 pipe.response[0]=0;args.text="9 1 101 Bad";mega_job_command(&pipe,&args,&bridge);assert(bridge.percent==50);
 pipe.available=0;pipe.response[0]=0;pipe.response[0]=0;args.text="10 0";mega_files_command(&pipe,&args,&bridge);assert(!pipe.response[0]);
 puts("Actual firmware listing, read, CRC, path, offset, full-pipe and job-status checks passed.");
}
"""
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp) / "test.c"
    path.write_text(header + source + tests)
    binary = Path(temp) / "test"
    subprocess.run(
        [
            "cc",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-misleading-indentation",
            str(path),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
