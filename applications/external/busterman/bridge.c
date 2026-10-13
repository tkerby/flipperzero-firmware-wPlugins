#include "bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define CAPTURE_DIR "/ext/apps_data/marauder/pcaps"
#define MAX_CAPTURE (16U * 1024U * 1024U)
#define CHUNK 256
static void reply(PipeSide* pipe, const char* text) {
    size_t n = strlen(text), sent = 0;
    uint32_t start = furi_get_tick();
    /* mntm-012 VCP pipe holds only 3 USB packets (192 bytes).
     * Stream large replies without holding the GUI state mutex. A bounded
     * timeout also covers a host that remains connected but stops draining. */
    while(sent < n && furi_get_tick() - start < furi_ms_to_ticks(500)) {
        size_t room = pipe_spaces_available(pipe);
        if(room) {
            size_t count = n - sent < room ? n - sent : room;
            size_t wrote = pipe_send(pipe, text + sent, count);
            sent += wrote;
            if(!wrote) furi_delay_ms(2);
        } else furi_delay_ms(2);
    }
}
static bool capture_name(const char* name) {
    size_t n = strlen(name);
    if(!n || n > 120 || strchr(name, '/') || strchr(name, '\\')) return false;
    for(size_t i=0; i<n; i++) if((unsigned char)name[i]<32) return false;
    return (n>=5 && !strcmp(name+n-5,".pcap")) || (n>=7 && !strcmp(name+n-7,".pcapng"));
}
static void hex(char* output, const unsigned char* data, size_t length) {
    const char* digits="0123456789abcdef";
    for(size_t i=0;i<length;i++) { output[2*i]=digits[data[i]>>4]; output[2*i+1]=digits[data[i]&15]; }
    output[2*length]=0;
}
static int nibble(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
static uint32_t crc32(const unsigned char* bytes, size_t n) {
    uint32_t crc=0xffffffff;
    for(size_t i=0;i<n;i++) { crc^=bytes[i]; for(unsigned k=0;k<8;k++) crc=(crc>>1)^((crc&1)?0xedb88320:0); }
    return ~crc;
}
static void mega_files_execute(PipeSide* pipe, FuriString* args, void* context) {
    MegaBridge* bridge=context; unsigned long request,index; char extra;
    if(sscanf(furi_string_get_cstr(args), "%lu %lu %c", &request,&index,&extra)!=2 || index>1024) return;
    char response[360], name[128];
    snprintf(response,sizeof(response),"[MEGAMAN_FILES] %lu END\r\n",request);
    File* directory=storage_file_alloc(bridge->storage);
    if(storage_dir_open(directory,CAPTURE_DIR)) {
        FileInfo info; unsigned long current=0;
        while(storage_dir_read(directory,&info,name,sizeof(name))) {
            if((info.flags & FSF_DIRECTORY) || !capture_name(name)) continue;
            if(current++!=index) continue;
            char encoded[241]; hex(encoded,(unsigned char*)name,strlen(name));
            snprintf(response,sizeof(response),"[MEGAMAN_FILES] %lu FILE %lu %s\r\n",request,(unsigned long)info.size,encoded);
            break;
        }
    }
    storage_dir_close(directory); storage_file_free(directory); reply(pipe,response);
}
static void mega_read_execute(PipeSide* pipe, FuriString* args, void* context) {
    MegaBridge* bridge=context; unsigned long request,offset; char encoded_name[241],extra;
    if(sscanf(furi_string_get_cstr(args),"%lu %240s %lu %c",&request,encoded_name,&offset,&extra)!=3) return;
    char response[640]; snprintf(response,sizeof(response),"[MEGAMAN_READ] %lu ERROR invalid-file\r\n",request);
    size_t n=strlen(encoded_name); char name[121]; bool valid=n>0 && !(n%2);
    for(size_t i=0;valid && i<n/2;i++) {
        int hi=nibble(encoded_name[2*i]),lo=nibble(encoded_name[2*i+1]);
        if(hi<0||lo<0||(hi==0&&lo==0)) valid=false;
        else name[i]=(char)((hi<<4)|lo);
    }
    name[n/2]=0;
    if(valid && capture_name(name)) {
        char path[180]; snprintf(path,sizeof(path),"%s/%s",CAPTURE_DIR,name);
        File* file=storage_file_alloc(bridge->storage);
        if(storage_file_open(file,path,FSAM_READ,FSOM_OPEN_EXISTING)) {
            uint64_t size=storage_file_size(file);
            if(size>0 && size<=MAX_CAPTURE && offset<=size && storage_file_seek(file,offset,true)) {
                unsigned char bytes[CHUNK];
                size_t wanted=size-offset<CHUNK?(size_t)(size-offset):CHUNK;
                size_t got=storage_file_read(file,bytes,wanted);
                if(got==wanted) {
                    char encoded[2*CHUNK+1]; hex(encoded,bytes,got);
                    snprintf(response,sizeof(response),"[MEGAMAN_READ] %lu DATA %lu %lu %lu %08lx %s\r\n",request,
                        (unsigned long)size,offset,(unsigned long)got,(unsigned long)crc32(bytes,got),got?encoded:"-");
                }
            } else snprintf(response,sizeof(response),"[MEGAMAN_READ] %lu ERROR size-or-offset\r\n",request);
        }
        storage_file_close(file); storage_file_free(file);
    }
    reply(pipe,response);
}
static void mega_job_execute(PipeSide* pipe, FuriString* args, void* context) {
    MegaBridge* bridge=context; unsigned long request; unsigned state; int percent; char label[32],extra;
    if(sscanf(furi_string_get_cstr(args),"%lu %u %d %31s %c",&request,&state,&percent,label,&extra)!=4 ||
       state>4 || percent< -1 || percent>100) return;
    bridge->state=state; bridge->percent=percent; snprintf(bridge->label,sizeof(bridge->label),"%s",label);
    char response[96]; snprintf(response,sizeof(response),"[MEGAMAN_JOB] %lu OK\r\n",request); reply(pipe,response);
}

void mega_files_command(PipeSide* pipe, FuriString* args, void* context) {
    MegaBridge* bridge = context;
    furi_mutex_acquire(bridge->lock, FuriWaitForever);
    mega_files_execute(pipe, args, context);
    furi_mutex_release(bridge->lock);
}

void mega_read_command(PipeSide* pipe, FuriString* args, void* context) {
    MegaBridge* bridge = context;
    furi_mutex_acquire(bridge->lock, FuriWaitForever);
    mega_read_execute(pipe, args, context);
    furi_mutex_release(bridge->lock);
}

void mega_job_command(PipeSide* pipe, FuriString* args, void* context) {
    MegaBridge* bridge = context;
    furi_mutex_acquire(bridge->lock, FuriWaitForever);
    mega_job_execute(pipe, args, context);
    furi_mutex_release(bridge->lock);
}
