#!/usr/bin/env python3
"""Compile extracted production C against fault-injected hardware/storage stand-ins.
Usage: python3 verify_updates.py /path/to/RM
These host tests do not transmit RF or simulate the RTOS/physical radio.
"""
from pathlib import Path
import os,re,subprocess,sys,tempfile
RM=Path(sys.argv[1]).resolve()
APPS=RM/'applications/external'

def fun(src,name):
    m=re.search(r'^[\w* \t]+\b'+re.escape(name)+r'\s*\(',src,re.M)
    assert m,name
    start=m.start()
    if src[start:m.end()].lstrip().startswith(name+'('):
        start=src.rfind('\n',0,start-1)+1
    masked=re.sub(r'/\*[\s\S]*?\*/|//[^\n]*',lambda token:' '*len(token.group()),src)
    brace=masked.index('{',m.end());end=brace+1;depth=1
    while depth:
        depth+=(masked[end]=='{')-(masked[end]=='}');end+=1
    return src[start:end]+'\n'

def read(path):return (APPS/path).read_text()
COMMON=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#define furi_assert assert
#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)
#define MIN(a,b) ((a)<(b)?(a):(b))
#define UNUSED(x) ((void)(x))
static unsigned checks;
#define CHECK(x) do { ++checks; assert(x); } while(0)
typedef struct { char text[1024]; } FuriString;
static FuriString* furi_string_alloc(void){return calloc(1,sizeof(FuriString));}
static FuriString* furi_string_alloc_printf(const char* fmt,...){FuriString* s=furi_string_alloc();va_list ap;va_start(ap,fmt);vsnprintf(s->text,sizeof(s->text),fmt,ap);va_end(ap);return s;}
static void furi_string_free(FuriString* s){free(s);}
static const char* furi_string_get_cstr(const FuriString* s){return s->text;}
static uint32_t ticks;
static uint32_t furi_get_tick(void){return ticks;}
static uint32_t furi_ms_to_ticks(uint32_t ms){return ms;}
static uint32_t furi_kernel_get_tick_frequency(void){return 1000;}
'''
TX=r'''
typedef struct {int n;} FlipperFormat;
typedef struct {FuriString* name; uint32_t frequency;} SubGhzRadioPreset;
enum {SubGhzTxRxStateIDLE,SubGhzTxRxStateTx,SubGhzTxRxStateSleep};
typedef enum {SubGhzTxRxStartTxStateOk,SubGhzTxRxStartTxStateErrorOnlyRx,SubGhzTxRxStartTxStateErrorParserOthers} SubGhzTxRxStartTxState;
enum {SubGhzProtocolStatusOk};
typedef struct {int txrx_state;SubGhzRadioPreset* preset;void* transmitter;void* setting;void* environment;void* radio_device;FlipperFormat* fff_data;bool tx_from_internal_fff;} SubGhzTxRx;
typedef SubGhzTxRx SubGhzWarDrivingTxRx;
static int fail,allocs,frees,starts,stops,pipelines,polls;
static uint32_t repeat;
static bool complete;
static bool flipper_format_rewind(FlipperFormat* f){UNUSED(f);return fail!=1;}
static bool flipper_format_read_string(FlipperFormat* f,const char* k,FuriString* s){UNUSED(f);UNUSED(k);strcpy(s->text,"RAW");return fail!=2;}
static bool flipper_format_insert_or_update_uint32(FlipperFormat* f,const char* k,uint32_t* r,int n){UNUSED(f);UNUSED(k);UNUSED(n);repeat=*r;return fail!=3;}
static void* subghz_transmitter_alloc_init(void* e,const char* n){UNUSED(e);UNUSED(n);if(fail==4)return NULL;++allocs;return malloc(1);}
static int subghz_transmitter_deserialize(void* t,FlipperFormat* f){CHECK(t!=NULL);UNUSED(f);return fail==5?9:SubGhzProtocolStatusOk;}
static void subghz_transmitter_free(void* t){CHECK(t!=NULL);++frees;free(t);}
static void subghz_transmitter_yield(void){}
static uint8_t* subghz_setting_get_preset_data_by_name(void* s,const char* n){UNUSED(s);UNUSED(n);static uint8_t p;return fail==6?NULL:&p;}
static void subghz_txrx_idle(SubGhzTxRx* t){t->txrx_state=SubGhzTxRxStateIDLE;}
static void subghz_txrx_begin(SubGhzTxRx* t,uint8_t* p){CHECK(p!=NULL);subghz_txrx_idle(t);}
static bool subghz_txrx_tx(SubGhzTxRx* t,uint32_t f){UNUSED(f);if(fail==8)return false;t->txrx_state=SubGhzTxRxStateTx;return true;}
static bool subghz_devices_start_async_tx(void* d,void(*cb)(void),void* ctx){UNUSED(d);UNUSED(cb);CHECK(ctx!=NULL);++starts;return fail!=9;}
static bool subghz_devices_is_async_complete_tx(void* d){UNUSED(d);++polls;return complete;}
static void subghz_txrx_stop(SubGhzTxRx* t){if(t->txrx_state==SubGhzTxRxStateTx){++stops;subghz_transmitter_free(t->transmitter);t->transmitter=NULL;}subghz_txrx_idle(t);}
static void subghz_wardriving_txrx_rx_pipeline_alloc(SubGhzTxRx* t){UNUSED(t);++pipelines;}
#define subghz_wardriving_txrx_stop subghz_txrx_stop
#define subghz_wardriving_txrx_idle subghz_txrx_idle
#define subghz_wardriving_txrx_begin subghz_txrx_begin
#define subghz_wardriving_txrx_tx subghz_txrx_tx
'''
TX_MAIN=r'''
int main(void){
 for(int mode=0;mode<10;mode++){
  if(!IS_MEAL && mode==3)continue;
  fail=mode;allocs=frees=starts=stops=0;repeat=0;
  FuriString name={"AM650"};SubGhzRadioPreset preset={&name,fail==7?0:433920000};
  FlipperFormat format={0},history={0};SubGhzTxRx t={.preset=&preset,.fff_data=&format};
  SubGhzTxRxStartTxState result=TEST_START(&t,&format);
  CHECK((result==SubGhzTxRxStartTxStateOk)==(mode==0));
  if(mode){CHECK(t.transmitter==NULL);CHECK(t.txrx_state!=SubGhzTxRxStateTx);CHECK(stops==0);}
  else {CHECK(t.transmitter!=NULL);CHECK(t.txrx_state==SubGhzTxRxStateTx);}
  if(IS_MEAL && mode>2)CHECK(repeat==200);
  if(IS_WARD)CHECK(t.tx_from_internal_fff);
  subghz_txrx_stop(&t);CHECK(allocs==frees);
  fail=0;preset.frequency=433920000;CHECK(TEST_START(&t,&history)==SubGhzTxRxStartTxStateOk);
  if(IS_WARD)CHECK(!t.tx_from_internal_fff);
  subghz_txrx_stop(&t);CHECK(allocs==frees);CHECK(t.transmitter==NULL);
 }
 printf("start/cleanup cases: %u assertions passed\n",checks);return 0;
}
'''
PROTO=r'''
typedef enum {TxRxStateIDLE,TxRxStateRx,TxRxStateTx,TxRxStateSleep} TxRxState;
typedef enum {ProtoViewTxIdle,ProtoViewTxPending,ProtoViewTxComplete,ProtoViewTxTimeout} ProtoViewTxResult;
typedef struct {TxRxState txrx_state,tx_previous_state;void* tx_context;uint32_t tx_deadline;} ProtoViewTxRx;
typedef struct {ProtoViewTxRx* txrx;void* radio_device;uint32_t frequency;} ProtoViewApp;
typedef void(*FuriHalSubGhzAsyncTxCallback)(void);
static bool start_ok,complete;
static int starts,stops,rx_starts,rx_stops;
static void radio_begin(ProtoViewApp* a){a->txrx->txrx_state=TxRxStateIDLE;}
static void radio_rx_end(ProtoViewApp* a){++rx_stops;radio_begin(a);}
static void radio_rx(ProtoViewApp* a){++rx_starts;a->txrx->txrx_state=TxRxStateRx;}
static void radio_sleep(ProtoViewApp* a){a->txrx->txrx_state=TxRxStateSleep;}
static void subghz_devices_idle(void* d){UNUSED(d);}
static uint32_t subghz_devices_set_frequency(void* d,uint32_t f){UNUSED(d);return f;}
static bool subghz_devices_start_async_tx(void* d,FuriHalSubGhzAsyncTxCallback cb,void* ctx){UNUSED(d);UNUSED(cb);CHECK(*(int*)ctx==123);++starts;return start_ok;}
static bool subghz_devices_is_async_complete_tx(void* d){UNUSED(d);return complete;}
static void subghz_devices_stop_async_tx(void* d){UNUSED(d);++stops;}
'''
PROTO_MAIN=r'''
int main(void){
 for(int old=0;old<4;++old){if(old==TxRxStateTx)continue;
  for(int mode=0;mode<5;++mode){
   start_ok=mode!=0;complete=false;starts=stops=rx_starts=rx_stops=0;ticks=UINT32_MAX-50;
   ProtoViewTxRx t={.txrx_state=old};ProtoViewApp a={.txrx=&t,.frequency=433920000};
   int* context=malloc(sizeof(int));*context=123;
   bool ok=radio_tx_signal(&a,NULL,context,100);
   CHECK(ok==start_ok);CHECK(starts==1);
   if(!ok){CHECK(t.txrx_state==old);CHECK(*context==123);free(context);CHECK(stops==0);continue;}
   CHECK(*(int*)t.tx_context==123);CHECK(t.txrx_state==TxRxStateTx);
   CHECK(radio_tx_poll(&a)==ProtoViewTxPending);
   ticks+=99;CHECK(radio_tx_poll(&a)==ProtoViewTxPending);
   if(mode==1){complete=true;CHECK(radio_tx_poll(&a)==ProtoViewTxComplete);}
   else if(mode==2){ticks++;CHECK(radio_tx_poll(&a)==ProtoViewTxTimeout);}
   else radio_tx_stop(&a,mode==3);
   CHECK(stops==1);CHECK(t.tx_context==NULL);CHECK(t.txrx_state==(mode==4?TxRxStateIDLE:old));
   radio_tx_stop(&a,true);CHECK(stops==1);CHECK(radio_tx_poll(&a)==ProtoViewTxIdle);
  }
 }
 printf("ProtoView lifecycle/wrap cases: %u assertions passed\n",checks);
}
'''
# Minimal radio model shared by the Genie and FlipBoard tests.
RADIO=r'''
typedef struct {int n;} SubGhzDevice;
typedef struct {int n;} SubGhzEnvironment;
typedef struct {int type;} SubGhzProtocol;
typedef struct {const SubGhzProtocol* protocol;} SubGhzProtocolEncoderBase;
typedef struct {SubGhzProtocolEncoderBase base;} SubGhzTransmitter;
typedef struct {uint8_t level;uint32_t duration;} LevelDuration;
static LevelDuration level_duration_make(bool level,uint32_t duration){return (LevelDuration){level?2:1,duration};}
static LevelDuration level_duration_reset(void){return (LevelDuration){0,0};}
static bool level_duration_is_reset(LevelDuration ld){return ld.level==0;}
static bool level_duration_is_wait(LevelDuration ld){return ld.level==3;}
static uint32_t level_duration_get_duration(LevelDuration ld){return ld.duration;}
enum {SubGhzProtocolStatusOk,SubGhzProtocolTypeDynamic=2,FuriHalSubGhzPresetOok650Async=1,FuriFlagWaitAny=1,FuriFlagNoClear=2};
#define SUBGHZ_DEVICE_CC1101_INT_NAME "cc1101_int"
#define SUBGHZ_DEVICE_CC1101_EXT_NAME "cc1101_ext"
#define SUBGHZ_KEYSTORE_DIR_NAME "keys"
#define SUBGHZ_KEYSTORE_DIR_USER_NAME "userkeys"
#define SUBGHZ_CAME_ATOMO_DIR_NAME "came"
#define SUBGHZ_ALUTECH_AT_4N_DIR_NAME "alutech"
#define SUBGHZ_NICE_FLOR_S_DIR_NAME "nice"
static int mode,allocs,frees,starts,stops,power,devices,begins,ends;
static uint32_t flags,start_tick;
static bool dynamic;
static int send_subghz_protocol_registry,subghz_protocol_registry;
static SubGhzDevice device;
static SubGhzProtocol protocol;
static LevelDuration yield_sample;
static uint32_t furi_thread_flags_get(void){return flags;}
static uint32_t furi_thread_flags_wait(uint32_t f,uint32_t options,uint32_t timeout){UNUSED(f);UNUSED(options);ticks+=timeout;if(mode==8 && (uint32_t)(ticks-start_tick)>=20)flags=1;CHECK((uint32_t)(ticks-start_tick)<10000);return flags;}
static void furi_delay_ms(uint32_t n){furi_thread_flags_wait(1,0,n);}
static void subghz_devices_init(void){++devices;}
static void subghz_devices_deinit(void){--devices;}
static const SubGhzDevice* subghz_devices_get_by_name(const char* n){UNUSED(n);return mode==1?NULL:&device;}
static const char* subghz_devices_get_name(const SubGhzDevice* d){CHECK(d);return "cc1101_int";}
static bool subghz_devices_is_frequency_valid(const SubGhzDevice* d,uint32_t f){CHECK(d);UNUSED(f);return mode!=2;}
static SubGhzEnvironment* subghz_environment_alloc(void){return calloc(1,sizeof(SubGhzEnvironment));}
static void subghz_environment_free(SubGhzEnvironment* e){free(e);}
static void subghz_environment_set_protocol_registry(SubGhzEnvironment* e,void* r){UNUSED(e);UNUSED(r);}
static void subghz_environment_load_keystore(SubGhzEnvironment* e,const char* p){UNUSED(e);UNUSED(p);}
#define subghz_environment_set_came_atomo_rainbow_table_file_name subghz_environment_load_keystore
#define subghz_environment_set_alutech_at_4n_rainbow_table_file_name subghz_environment_load_keystore
#define subghz_environment_set_nice_flor_s_rainbow_table_file_name subghz_environment_load_keystore
static SubGhzTransmitter* subghz_transmitter_alloc_init(SubGhzEnvironment* e,const char* n){UNUSED(e);UNUSED(n);if(mode==3)return NULL;++allocs;SubGhzTransmitter* t=calloc(1,sizeof(*t));protocol.type=dynamic?SubGhzProtocolTypeDynamic:0;t->base.protocol=&protocol;return t;}
static SubGhzProtocolEncoderBase* subghz_transmitter_get_protocol_instance(SubGhzTransmitter* t){CHECK(t);return &t->base;}
static void subghz_transmitter_stop(SubGhzTransmitter* t){CHECK(t);}
static void subghz_transmitter_free(SubGhzTransmitter* t){CHECK(t);++frees;free(t);}
static LevelDuration subghz_transmitter_yield(void* t){CHECK(t);return yield_sample;}
static void subghz_devices_begin(const SubGhzDevice* d){CHECK(d);++begins;}
static void subghz_devices_end(const SubGhzDevice* d){CHECK(d);++ends;}
static void subghz_devices_reset(const SubGhzDevice* d){CHECK(d);}
static void subghz_devices_sleep(const SubGhzDevice* d){CHECK(d);}
static void subghz_devices_load_preset(const SubGhzDevice* d,int p,uint8_t* data){CHECK(d);UNUSED(p);UNUSED(data);}
static uint32_t subghz_devices_set_frequency(const SubGhzDevice* d,uint32_t f){CHECK(d);return f;}
static bool subghz_devices_start_async_tx(const SubGhzDevice* d,LevelDuration(*cb)(void*),void* ctx){CHECK(d);CHECK(ctx);UNUSED(cb);++starts;start_tick=ticks;return mode!=5;}
static bool subghz_devices_is_async_complete_tx(const SubGhzDevice* d){CHECK(d);return mode!=7 && mode!=8 && (uint32_t)(ticks-start_tick)>=40;}
static void subghz_devices_stop_async_tx(const SubGhzDevice* d){CHECK(d);++stops;}
static void furi_hal_power_suppress_charge_enter(void){++power;}
static void furi_hal_power_suppress_charge_exit(void){--power;}
static bool furi_hal_power_is_otg_enabled(void){return false;}
static void subghz_radio_device_power_on(void){}
static void subghz_radio_device_power_off(void){}
static void reset_radio(int m){mode=m;allocs=frees=starts=stops=power=devices=begins=ends=0;flags=m==6?1:0;ticks=UINT32_MAX-30;start_tick=ticks;yield_sample=level_duration_reset();}
'''
GENIE=r'''
#define GENIE_SEND_CANCEL 1U
#define GENIE_SEND_TIMEOUT_MS 5000U
typedef enum {GenieTxFailed,GenieTxComplete,GenieTxCancelled,GenieTxTimeout} GenieTxResult;
typedef struct {int n;} FlipperFormat;
static FlipperFormat* flipper_format_string_alloc(void){return calloc(1,sizeof(FlipperFormat));}
static void flipper_format_free(FlipperFormat* f){free(f);}
static void set_genie(FlipperFormat* f,uint32_t hi,uint32_t lo,uint32_t repeat){UNUSED(f);CHECK(hi==12 && lo==34 && repeat==5);}
static int subghz_transmitter_deserialize(SubGhzTransmitter* t,FlipperFormat* f){CHECK(t);UNUSED(f);return mode==4?9:SubGhzProtocolStatusOk;}
typedef struct {void* file;bool loaded,sending;const char* status;} GenieRefs;
typedef struct {GenieRefs* view;FuriString* path;uint32_t key_hi,key_lo,frequency;uint16_t next_index;bool pending_save;} GenieSend;
#define with_view_model(view,type,code,redraw) do {type=(view);code;} while(0)
static bool save_ok;
static int saves,refreshes;
static bool genie_file_set_last_sent(const char* path,uint16_t index){CHECK(!strcmp(path,"test.GNE"));CHECK(index==9);++saves;return save_ok;}
static void genie_send_refresh_file(GenieSend* s){UNUSED(s);++refreshes;}
'''
GENIE_MAIN=r'''
int main(void){
 for(int m=0;m<9;++m){
  reset_radio(m);saves=refreshes=0;save_ok=true;
  FuriString path={"test.GNE"};GenieRefs view={.sending=true};GenieSend sender={.view=&view,.path=&path,.key_hi=12,.key_lo=34,.frequency=433920000,.next_index=9};
  genie_send_worker(&sender);
  CHECK(saves==(m==0));CHECK(!view.sending);CHECK(!sender.pending_save);
  CHECK(allocs==frees);CHECK(!power && !devices);CHECK(begins==ends);
  CHECK(stops==((m==0 || m==7 || m==8)?1:0));
  if(m)CHECK(view.status!=NULL);
 }
 reset_radio(0);saves=0;save_ok=false;
 FuriString path={"test.GNE"};GenieRefs view={.sending=true};GenieSend sender={.view=&view,.path=&path,.key_hi=12,.key_lo=34,.frequency=433920000,.next_index=9};
 genie_send_worker(&sender);CHECK(sender.pending_save);CHECK(starts==1);CHECK(saves==1);
 CHECK(!genie_send_retry_save(&sender));CHECK(starts==1);CHECK(saves==2);
 save_ok=true;CHECK(genie_send_retry_save(&sender));CHECK(!sender.pending_save);CHECK(starts==1);CHECK(saves==3);CHECK(view.status==NULL);
 printf("Genie TX/index/retry cases: %u assertions passed\n",checks);
}
'''

cases=[]
for app,path,name in [
 ('Sub-GHz Remote','subghz_remote/helpers/txrx/subghz_txrx.c','subghz_txrx_tx_start'),
 ('WarDriving','subghz_wardriving/helpers/subghz_wardriving_txrx.c','subghz_wardriving_txrx_tx_start'),
 ('Restaurant Pager','meal_pager/helpers/subghz/subghz_txrx.c','subghz_txrx_tx_start')]:
 defs=f'#define IS_MEAL {int(app=="Restaurant Pager")}\n#define IS_WARD {int(app=="WarDriving")}\n#define TEST_START {name}\n'
 cases.append((app,COMMON+TX+defs+fun(read(path),name)+TX_MAIN))
s=read('protoview/app_subghz.c')
cases.append(('ProtoView',COMMON+PROTO+''.join(fun(s,n) for n in ['radio_tx_signal','radio_tx_stop','radio_tx_poll'])+PROTO_MAIN))
s=read('genie_recorder/genie_send.c')
cases.append(('Genie',COMMON+RADIO+GENIE+''.join(fun(s,n) for n in ['genie_send_cancelled','send_genie','genie_send_worker','genie_send_retry_save'])+GENIE_MAIN))
FS=r'''
#define RECORD_STORAGE 1
#define FSAM_WRITE 1
#define FSOM_CREATE_ALWAYS 1
#define FSE_OK 0
#define StreamOffsetFromStart 0
typedef struct {int unused;} Storage;
typedef struct {bool exists;size_t len;uint8_t bytes[1024];} DiskEntry;
static DiskEntry disk[4]; // original, temporary, pending, backup
static Storage storage;
typedef struct {int idx;bool opened;size_t offset;} File;
typedef struct {uint8_t bytes[1024];size_t len,pos;} Stream;
typedef struct {Stream stream;} FlipperFormat;
static int fs_fail,rename_calls,remove_calls,file_allocs,file_frees,records;
static int path_id(const char* p){if(strstr(p,".flip.tmp"))return 1;if(strstr(p,".flip.pending"))return 2;if(strstr(p,".flip.bak"))return 3;return 0;}
static void* furi_record_open(int id){UNUSED(id);++records;return &storage;}
static void furi_record_close(int id){UNUSED(id);--records;}
static bool storage_file_exists(Storage* s,const char* p){UNUSED(s);return disk[path_id(p)].exists;}
static int storage_common_rename_safe(Storage* s,const char* from,const char* to){UNUSED(s);++rename_calls;if((fs_fail==10 && rename_calls==1)||(fs_fail==11 && rename_calls==2))return 5;int a=path_id(from),b=path_id(to);if(!disk[a].exists||disk[b].exists)return 6;disk[b]=disk[a];memset(&disk[a],0,sizeof(disk[a]));return FSE_OK;}
static int storage_common_remove(Storage* s,const char* path){UNUSED(s);++remove_calls;if(fs_fail==12)return 5;int i=path_id(path);if(!disk[i].exists)return 6;memset(&disk[i],0,sizeof(disk[i]));return FSE_OK;}
static File* storage_file_alloc(Storage* s){UNUSED(s);++file_allocs;return calloc(1,sizeof(File));}
static void storage_file_free(File* f){CHECK(!f->opened);++file_frees;free(f);}
static bool storage_file_open(File* f,const char* p,int access,int mode){UNUSED(access);UNUSED(mode);if(fs_fail==1)return false;f->idx=path_id(p);f->opened=true;f->offset=0;disk[f->idx].exists=true;disk[f->idx].len=0;return true;}
static size_t storage_file_write(File* f,const void* buf,size_t n){CHECK(f->opened);if(fs_fail==3)n=n?n-1:0;CHECK(f->offset+n<=1024);memcpy(disk[f->idx].bytes+f->offset,buf,n);f->offset+=n;disk[f->idx].len=f->offset;return n;}
static bool storage_file_sync(File* f){CHECK(f->opened);return fs_fail!=4;}
static bool storage_file_close(File* f){bool opened=f->opened;f->opened=false;return opened&&fs_fail!=5;}
static Stream* flipper_format_get_raw_stream(FlipperFormat* f){return &f->stream;}
static size_t stream_size(Stream* s){return s->len;}
static bool stream_seek(Stream* s,int32_t pos,int origin){UNUSED(origin);s->pos=pos;return fs_fail!=2;}
static size_t stream_read(Stream* s,void* out,size_t n){if(fs_fail==6)n=n?n-1:0;CHECK(s->pos+n<=s->len);memcpy(out,s->bytes+s->pos,n);s->pos+=n;return n;}
static void reset_fs(void){memset(disk,0,sizeof(disk));fs_fail=rename_calls=remove_calls=file_allocs=file_frees=records=0;disk[0].exists=true;disk[0].len=3;memcpy(disk[0].bytes,"old",3);}
static FlipperFormat* flipper_format_string_alloc(void){return calloc(1,sizeof(FlipperFormat));}
static void flipper_format_free(FlipperFormat* f){free(f);}
static void __flipper_format_populate(FlipperFormat* f,FuriString* s){f->stream.len=strlen(s->text);memcpy(f->stream.bytes,s->text,f->stream.len);}
static bool flipper_format_update_string_cstr(FlipperFormat* f,const char* k,const char* v){UNUSED(f);UNUSED(k);UNUSED(v);return true;}
static bool furi_string_equal(FuriString* a,const char* b){return !strcmp(a->text,b);}
'''
FLIP=r'''
typedef enum {SubGhzSignalError,SubGhzSignalComplete,SubGhzSignalCancelled,SubGhzSignalTimeout,SubGhzSignalSaveError} SubGhzSignalResult;
typedef struct {FuriString* sub_file_contents;FuriString* protocol;FuriString* file_path;uint32_t frequency;int preset;uint8_t* preset_data;} SubGhzSignal;
static int subghz_transmitter_deserialize(SubGhzTransmitter* t,FlipperFormat* f){CHECK(t);if(mode==4)return 9;if(dynamic){memcpy(f->stream.bytes,"new",3);f->stream.len=3;}return SubGhzProtocolStatusOk;}
'''
FLIP_MAIN=r'''
int main(void){
 // Stage several chunks; inject failures before the pending marker is created.
 for(int failure=0;failure<=6;++failure){
  reset_fs();fs_fail=failure;FlipperFormat fmt={0};fmt.stream.len=700;memset(fmt.stream.bytes,'N',700);
  bool ok=subghz_signal_stage(&fmt,&storage,"test.sub");CHECK(ok==(failure==0));CHECK(disk[0].exists && disk[0].len==3 && !memcmp(disk[0].bytes,"old",3));
  CHECK(disk[2].exists==ok);if(ok){CHECK(disk[2].len==700);CHECK(disk[2].bytes[699]=='N');}
  CHECK(file_allocs==file_frees);
 }
 // Recovery after each rename/removal failure must preserve the updated state.
 for(int failure=0;failure<4;++failure){
  reset_fs();disk[2].exists=true;disk[2].len=3;memcpy(disk[2].bytes,"new",3);
  fs_fail=failure?failure+9:0;
  bool ok=subghz_signal_recover(&storage,"test.sub");CHECK(ok==(failure==0));
  if(!ok)CHECK(disk[2].exists || (disk[0].exists&&!memcmp(disk[0].bytes,"new",3)));
  fs_fail=0;CHECK(subghz_signal_recover(&storage,"test.sub"));CHECK(disk[0].exists&&!memcmp(disk[0].bytes,"new",3));CHECK(!disk[2].exists&&!disk[3].exists);
 }
 reset_fs();disk[1].exists=true;disk[1].len=1;disk[1].bytes[0]='X';CHECK(subghz_signal_recover(&storage,"test.sub"));CHECK(!memcmp(disk[0].bytes,"old",3));
 reset_fs();disk[3]=disk[0];memset(&disk[0],0,sizeof(disk[0]));CHECK(subghz_signal_recover(&storage,"test.sub"));CHECK(disk[0].exists&&!disk[3].exists);
 reset_fs();disk[2]=disk[0];disk[3]=disk[0];CHECK(!subghz_signal_recover(&storage,"test.sub"));CHECK(disk[0].exists&&disk[2].exists&&disk[3].exists);
 // Full sender lifecycle, both static/RAW-style and dynamic protocols.
 for(int d=0;d<2;d++)for(int m=0;m<9;m++){
  reset_fs();reset_radio(m);dynamic=d;
  FuriString path={"test.sub"},contents={"old"},proto={"Princeton"};SubGhzSignal signal={.sub_file_contents=&contents,.protocol=&proto,.file_path=&path,.frequency=433920000,.preset=1};
  SubGhzSignalResult result=send_signal(&signal,false,1);
  CHECK(result==(m==0?SubGhzSignalComplete:m==6||m==8?SubGhzSignalCancelled:m==7?SubGhzSignalTimeout:SubGhzSignalError));
  CHECK(allocs==frees && !devices && !power && !records);CHECK(begins==ends);CHECK(file_allocs==file_frees);
  bool emitted=m==0||m==7||m==8;CHECK(stops==(emitted?1:0));
  CHECK(!memcmp(disk[0].bytes,d&&emitted?"new":"old",3));CHECK(!disk[2].exists&&!disk[3].exists);
 }
 reset_fs();reset_radio(0);dynamic=true;fs_fail=4;
 FuriString path={"test.sub"},contents={"old"},proto={"Princeton"};SubGhzSignal signal={.sub_file_contents=&contents,.protocol=&proto,.file_path=&path,.frequency=433920000,.preset=1};
 CHECK(send_signal(&signal,false,1)==SubGhzSignalSaveError);CHECK(starts==0);CHECK(!memcmp(disk[0].bytes,"old",3));
 // A burst of sub-tick pulses must not accumulate millisecond rounding error.
 reset_radio(0);ticks=UINT32_MAX-100;SubGhzTransmitter tx={0};SubGhzSignalFeed feed={.transmitter=&tx,.queued_until=ticks};
 yield_sample=level_duration_make(true,100);
 for(int i=0;i<1000;i++)subghz_signal_yield(&feed);
 CHECK(feed.queued_until==UINT32_MAX);CHECK(feed.tick_fraction==0);CHECK((uint32_t)(feed.deadline-ticks)==5101);
 yield_sample=level_duration_make(false,10000000);subghz_signal_yield(&feed);CHECK((uint32_t)(feed.deadline-ticks)==15101);
 uint32_t deadline=feed.deadline;yield_sample=level_duration_reset();subghz_signal_yield(&feed);CHECK(feed.deadline==deadline);
 printf("FlipBoard TX/journal/recovery/deadline cases: %u assertions passed\n",checks);
}
'''
s=read('flipsignal/common/subghz_signal.c')
feed=re.search(r'typedef struct \{\s*SubGhzTransmitter\* transmitter;.*?\} SubGhzSignalFeed;',s,re.S).group()
cases.append(('FlipBoard',COMMON+RADIO+FS+FLIP+feed+'\n'+''.join(fun(s,n) for n in ['subghz_signal_recover','subghz_signal_stage','subghz_signal_yield','subghz_signal_cancelled','send_signal'])+FLIP_MAIN))
# Snapshot the actual ProtoView feeder and verify it survives replacement of the original bitmap.
s=read('protoview/view_info.c')
feeder=s[s.index('typedef enum {\n    SendSignalSendStartGap'):s.index('/* Vibrate and produce a click sound')]
SNAP=r'''
typedef struct {uint32_t pulses_count,short_pulse_dur,bits_bytes;uint8_t* bits;} Info;
typedef struct {Info* msg_info;} ProtoViewApp;
typedef struct {uint8_t level;uint32_t duration;} LevelDuration;
static LevelDuration level_duration_make(bool l,uint32_t d){return (LevelDuration){l?2:1,d};}
static LevelDuration level_duration_reset(void){return (LevelDuration){0,0};}
static bool bitmap_get(uint8_t* b,uint32_t len,uint32_t bit){CHECK(bit/8<len);return (b[bit/8]>>(7-bit%8))&1;}
'''
SNAP_MAIN=r'''
int main(void){
 uint8_t bits=0xC3;Info info={8,100,1,&bits};ProtoViewApp app={&info};SendSignalCtx* ctx=malloc(sizeof(*ctx)+1);send_signal_init(ctx,&app);bits=0;app.msg_info=NULL;
 LevelDuration ld=radio_tx_feed_data(ctx);CHECK(ld.level==1&&ld.duration==10000);
 ld=radio_tx_feed_data(ctx);CHECK(ld.level==2&&ld.duration==200);
 ld=radio_tx_feed_data(ctx);CHECK(ld.level==1&&ld.duration==400);
 ld=radio_tx_feed_data(ctx);CHECK(ld.level==2&&ld.duration==200);
 ld=radio_tx_feed_data(ctx);CHECK(ld.level==1&&ld.duration==10000);
 CHECK(radio_tx_feed_data(ctx).level==0);CHECK(radio_tx_feed_data(ctx).level==0);free(ctx);
 printf("ProtoView waveform snapshot cases: %u assertions passed\n",checks);
}
'''
cases.append(('ProtoView snapshot',COMMON+SNAP+feeder+SNAP_MAIN))
INDEX=r'''
#define RECORD_STORAGE 1
#define GENIE_VERSION 2
#define GENIE_LAST_SENT 8
#define GENIE_MAJOR_VERSION 2
#define FSAM_READ_WRITE 1
#define FSOM_OPEN_EXISTING 1
typedef struct {int unused;} Storage;
typedef struct {bool opened;} File;
static int failure,records,allocs,frees,writes,seeks;
static Storage storage;
static void* furi_record_open(int id){UNUSED(id);++records;return &storage;}
static void furi_record_close(int id){UNUSED(id);--records;}
static File* storage_file_alloc(Storage* s){UNUSED(s);++allocs;return calloc(1,sizeof(File));}
static void storage_file_free(File* f){++frees;CHECK(!f->opened);free(f);}
static bool storage_file_exists(Storage* s,const char* p){UNUSED(s);UNUSED(p);return failure!=1;}
static bool storage_file_open(File* f,const char* p,int a,int m){UNUSED(p);UNUSED(a);UNUSED(m);f->opened=failure!=2;return f->opened;}
static bool storage_file_seek(File* f,int pos,bool from_start){CHECK(f->opened);UNUSED(pos);UNUSED(from_start);++seeks;return !(failure==3 && seeks==1) && !(failure==6 && seeks==2);}
static size_t storage_file_read(File* f,void* bytes,size_t len){CHECK(f->opened);CHECK(len==2);uint8_t* b=bytes;b[0]=failure==5?GENIE_MAJOR_VERSION+1:GENIE_MAJOR_VERSION;b[1]=0;return failure==4?1:2;}
static bool storage_file_write16(File* f,uint16_t index){CHECK(f->opened);CHECK(index==9);++writes;return failure!=7;}
static bool storage_file_sync(File* f){CHECK(f->opened);return failure!=8;}
static bool storage_file_close(File* f){f->opened=false;return failure!=9;}
'''
INDEX_MAIN=r'''
int main(void){
 for(failure=0;failure<10;failure++){
  records=allocs=frees=writes=seeks=0;
  CHECK(genie_file_set_last_sent("test.GNE",9)==(failure==0));CHECK(records==0);CHECK(allocs==frees);CHECK(writes==(failure==0||failure>=7));
 }
 CHECK(!genie_file_set_last_sent(NULL,9));
 printf("Genie index storage failures: %u assertions passed\n",checks);
}
'''
cases.append(('Genie storage',COMMON+INDEX+fun(read('genie_recorder/genie_file.c'),'genie_file_set_last_sent')+INDEX_MAIN))

with tempfile.TemporaryDirectory(prefix='rm_external6_tests_') as tmp:
    for i,(name,src) in enumerate(cases):
        path=Path(tmp)/f'case{i}.c';path.write_text(src);exe=path.with_suffix('')
        args=['cc','-std=gnu11','-g','-O1','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fsanitize=address,undefined','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-variable','-Wno-sign-compare',str(path),'-o',str(exe)]
        build=subprocess.run(args,text=True,capture_output=True)
        if build.returncode:
            print(name,build.stderr);raise SystemExit(build.returncode)
        if build.stderr:print(name,build.stderr)
        run=subprocess.run([str(exe)],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
        print(name+': '+run.stdout.strip(),flush=True)
        if run.returncode:
            print(run.stderr);raise SystemExit(run.returncode)
print('All extracted-production-C cases passed with ASan/UBSan and explicit resource-balance checks. LeakSanitizer is unavailable under this executor.')
