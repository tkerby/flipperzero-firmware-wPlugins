// Host shim for the Flipper firmware API.
//
// Purpose: run the app's REAL draw and input code on a PC so the UI can be inspected without a
// device. The radio is not simulated -- furi_hal_subghz_get_rssi() returns a synthetic spectrum
// so the worker has something to plot -- but everything above the HAL is the shipped code.
//
// The canvas does not rasterise. It prints a draw trace, and sim/render.py turns that into a
// PNG with a real font, which avoids shipping a 5x7 font table that would only approximate the
// device anyway.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#define UNUSED(x) ((void)(x))

// ------------------------------------------------------------------ logging
#define FURI_LOG_E(tag, ...) sim_log('E', (tag), __VA_ARGS__)
#define FURI_LOG_W(tag, ...) sim_log('W', (tag), __VA_ARGS__)
#define FURI_LOG_I(tag, ...) sim_log('I', (tag), __VA_ARGS__)
#define FURI_LOG_D(tag, ...) sim_log('D', (tag), __VA_ARGS__)
void sim_log(char level, const char* tag, const char* fmt, ...);

// ------------------------------------------------------------------ basic types
typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second, weekday;
} DateTime;

typedef struct FuriString FuriString;
typedef struct File File;
typedef struct Storage Storage;
typedef struct FlipperFormat FlipperFormat;
typedef struct FuriThread FuriThread;
typedef struct FuriTimer FuriTimer;
typedef struct FuriStreamBuffer FuriStreamBuffer;
typedef struct View View;
typedef struct ViewDispatcher ViewDispatcher;
typedef struct VariableItem VariableItem;
typedef struct VariableItemList VariableItemList;
typedef struct Gui Gui;
typedef struct NotificationApp NotificationApp;
typedef struct SubGhzDevice SubGhzDevice;
typedef struct SubGhzEnvironment SubGhzEnvironment;
typedef struct SubGhzReceiver SubGhzReceiver;
typedef struct SubGhzProtocolDecoderBase SubGhzProtocolDecoderBase;
typedef struct SubGhzProtocolRegistry {
    int unused;
} SubGhzProtocolRegistry;
typedef struct SubGhzProtocol SubGhzProtocol;
typedef struct SubGhzKeystore SubGhzKeystore;

typedef void* FuriRecordData;

// ------------------------------------------------------------------ threads / sync
typedef int32_t (*FuriThreadCallback)(void* context);
FuriThread* furi_thread_alloc_ex(const char* name, uint32_t stack, FuriThreadCallback cb, void* ctx);
void furi_thread_free(FuriThread* t);
void furi_thread_start(FuriThread* t);
bool furi_thread_join(FuriThread* t);
void furi_thread_yield(void);

FuriTimer* furi_timer_alloc(void (*cb)(void*), int type, void* ctx);
void furi_timer_free(FuriTimer* t);
void furi_timer_start(FuriTimer* t, uint32_t ms);
void furi_timer_stop(FuriTimer* t);
#define FuriTimerTypePeriodic 1

FuriStreamBuffer* furi_stream_buffer_alloc(size_t size, size_t trigger);
void furi_stream_buffer_free(FuriStreamBuffer* s);
size_t furi_stream_buffer_send(FuriStreamBuffer* s, const void* data, size_t len, uint32_t t);
size_t furi_stream_buffer_receive(FuriStreamBuffer* s, void* data, size_t len, uint32_t t);

uint32_t furi_get_tick(void);
void furi_delay_ms(uint32_t ms);
void* furi_record_open(const char* name);
void furi_record_close(const char* name);
#define RECORD_STORAGE "storage"
#define RECORD_GUI "gui"
#define RECORD_NOTIFICATION "notification"

// ------------------------------------------------------------------ strings
FuriString* furi_string_alloc(void);
void furi_string_free(FuriString* s);
const char* furi_string_get_cstr(FuriString* s);
int furi_string_cmp_str(FuriString* s, const char* c);

// ------------------------------------------------------------------ storage
typedef enum { FSAM_READ = 1, FSAM_WRITE = 2 } FS_AccessMode;
typedef enum { FSOM_OPEN_EXISTING = 1, FSOM_CREATE_ALWAYS = 2 } FS_OpenMode;
#define APP_DATA_PATH(x) "/data/" x
File* storage_file_alloc(Storage* s);
void storage_file_free(File* f);
bool storage_file_open(File* f, const char* path, FS_AccessMode m, FS_OpenMode o);
bool storage_file_close(File* f);
size_t storage_file_read(File* f, void* buf, size_t n);
size_t storage_file_write(File* f, const void* buf, size_t n);
bool storage_common_mkdir(Storage* s, const char* path);

// ------------------------------------------------------------------ flipper format
FlipperFormat* flipper_format_file_alloc(Storage* s);
void flipper_format_free(FlipperFormat* f);
bool flipper_format_file_open_always(FlipperFormat* f, const char* path);
bool flipper_format_file_open_existing(FlipperFormat* f, const char* path);
bool flipper_format_write_header_cstr(FlipperFormat* f, const char* h, uint32_t v);
bool flipper_format_read_header(FlipperFormat* f, FuriString* h, uint32_t* v);
bool flipper_format_write_uint32(FlipperFormat* f, const char* k, const uint32_t* d, uint16_t n);
bool flipper_format_write_int32(FlipperFormat* f, const char* k, const int32_t* d, uint16_t n);
bool flipper_format_read_uint32(FlipperFormat* f, const char* k, uint32_t* d, uint16_t n);
bool flipper_format_read_int32(FlipperFormat* f, const char* k, int32_t* d, uint16_t n);
bool flipper_format_write_string_cstr(FlipperFormat* f, const char* k, const char* d);
void flipper_format_rewind(FlipperFormat* f);

// ------------------------------------------------------------------ input
typedef enum {
    InputKeyUp = 1, InputKeyDown, InputKeyRight, InputKeyLeft, InputKeyOk, InputKeyBack,
    InputKeyMAX
} InputKey;
typedef enum { InputTypePress = 1, InputTypeRelease, InputTypeShort, InputTypeLong,
               InputTypeRepeat } InputType;
typedef struct {
    InputKey key;
    InputType type;
} InputEvent;

// ------------------------------------------------------------------ gui
typedef struct Canvas Canvas;
typedef enum { ColorWhite = 1, ColorBlack = 0, ColorXOR = 2 } Color;
typedef enum { FontPrimary = 0, FontSecondary = 1, FontKeyboard = 2, FontBigNumbers = 3 } Font;
typedef enum { AlignLeft = 0, AlignRight = 1, AlignCenter = 2, AlignTop = 0, AlignBottom = 1,
               AlignMiddle = 2 } Align;

void canvas_clear(Canvas* c);
void canvas_set_color(Canvas* c, Color col);
void canvas_set_font(Canvas* c, Font f);
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, size_t w, size_t h);
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, size_t w, size_t h);
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y);
void canvas_draw_line(Canvas* c, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void canvas_draw_disc(Canvas* c, int32_t x, int32_t y, size_t r);
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* s);
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align ha, Align va, const char* s);
size_t canvas_string_width(Canvas* c, const char* s);

void elements_button_left(Canvas* c, const char* s);
void elements_button_center(Canvas* c, const char* s);
void elements_button_right(Canvas* c, const char* s);

typedef enum {
    ViewModelTypeLocking = 0,
    ViewModelTypeLockFree = 1,
} ViewModelType;
View* view_alloc(void);
void view_free(View* v);
void view_allocate_model(View* v, ViewModelType t, size_t size);
void view_set_context(View* v, void* ctx);
void view_set_draw_callback(View* v, void (*cb)(Canvas*, void*));
void view_set_input_callback(View* v, bool (*cb)(InputEvent*, void*));
void view_set_previous_callback(View* v, uint32_t (*cb)(void*));
void view_set_enter_callback(View* v, void (*cb)(void*));
void view_set_exit_callback(View* v, void (*cb)(void*));
bool with_view_model(View* v, void** model, void* body, bool redraw);
// the real macro takes the view's model under a lock; the shim has no models, but the body
// still names the variable, so it has to exist
#define with_view_model(v, m, body, redraw)                                       \
    do {                                                                          \
        (void)(v);                                                                \
        (void)(redraw);                                                           \
        m = NULL; /* the argument is a declaration: "void** m" becomes "void** m = NULL" */  \
        body;                                                                     \
    } while(0)

typedef enum { ViewDispatcherTypeFullscreen = 0 } ViewDispatcherType;
ViewDispatcher* view_dispatcher_alloc(void);
void view_dispatcher_free(ViewDispatcher* vd);
void view_dispatcher_attach_to_gui(ViewDispatcher* vd, Gui* g, ViewDispatcherType t);
void view_dispatcher_add_view(ViewDispatcher* vd, uint32_t id, View* v);
void view_dispatcher_remove_view(ViewDispatcher* vd, uint32_t id);
void view_dispatcher_switch_to_view(ViewDispatcher* vd, uint32_t id);
void view_dispatcher_run(ViewDispatcher* vd);
#define VIEW_NONE 0xFFFFFFFF

typedef struct VariableItemList VariableItemList;
VariableItemList* variable_item_list_alloc(void);
void variable_item_list_free(VariableItemList* l);
View* variable_item_list_get_view(VariableItemList* l);
VariableItem* variable_item_list_add(
    VariableItemList* l, const char* label, uint8_t values, void (*cb)(VariableItem*), void* ctx);
void variable_item_list_set_enter_callback(
    VariableItemList* l, void (*cb)(void*, uint32_t), void* ctx);
void variable_item_set_current_value_index(VariableItem* i, uint8_t idx);
void variable_item_set_current_value_text(VariableItem* i, const char* text);
uint8_t variable_item_get_current_value_index(VariableItem* i);
void* variable_item_get_context(VariableItem* i);

// ------------------------------------------------------------------ notifications
typedef struct { uint32_t pattern; } NotificationSequence;
extern const NotificationSequence sequence_set_vibro_on;
extern const NotificationSequence sequence_reset_vibro;
void notification_message(NotificationApp* n, const NotificationSequence* s);

// ------------------------------------------------------------------ radio
typedef enum { FuriHalSubGhzPath315 = 0, FuriHalSubGhzPath433, FuriHalSubGhzPath868 } FuriHalSubGhzPath;
typedef struct {
    uint32_t duration : 30;
    uint8_t level : 2;
} LevelDuration;
typedef LevelDuration (*FuriHalSubGhzAsyncTxCallback)(void*);

void furi_hal_subghz_reset(void);
void furi_hal_subghz_idle(void);
void furi_hal_subghz_sleep(void);
uint32_t furi_hal_subghz_set_frequency(uint32_t f);
uint32_t furi_hal_subghz_set_frequency_and_path(uint32_t f);
void furi_hal_subghz_set_path(FuriHalSubGhzPath p);
bool furi_hal_subghz_is_frequency_valid(uint32_t f);
void furi_hal_subghz_rx(void);
void furi_hal_subghz_load_custom_preset(const uint8_t* regs);
float furi_hal_subghz_get_rssi(void);
void furi_hal_subghz_start_async_rx(void* cb, void* ctx);
void furi_hal_subghz_stop_async_rx(void);
bool furi_hal_subghz_start_async_tx(FuriHalSubGhzAsyncTxCallback cb, void* ctx);
void furi_hal_subghz_stop_async_tx(void);
bool furi_hal_subghz_is_async_tx_complete(void);

typedef struct {
    uint32_t start;
    uint32_t end;
    int8_t power_limit;
    uint16_t duty_cycle;
} FuriHalRegionBand;
typedef struct {
    char country_code[4];
    uint16_t bands_count;
    FuriHalRegionBand bands[3];
} FuriHalRegion;
const FuriHalRegion* furi_hal_region_get(void);
const char* furi_hal_region_get_name(void);
void furi_hal_region_set(FuriHalRegion* r);
bool furi_hal_region_is_frequency_allowed(uint32_t f);

void furi_hal_power_enable_otg(void);
void furi_hal_power_disable_otg(void);
bool furi_hal_power_is_otg_enabled(void);
uint8_t furi_hal_power_get_pct(void);

void furi_hal_rtc_get_datetime(DateTime* d);

// subghz device abstraction (external module). Never present in the sim.
const SubGhzDevice* subghz_devices_get_by_name(const char* n);
void subghz_devices_init(void);
void subghz_devices_deinit(void);
bool subghz_devices_is_connect(const SubGhzDevice* d);
void subghz_devices_begin(const SubGhzDevice* d);
void subghz_devices_end(const SubGhzDevice* d);
void subghz_devices_reset(const SubGhzDevice* d);
void subghz_devices_idle(const SubGhzDevice* d);
void subghz_devices_sleep(const SubGhzDevice* d);
void subghz_devices_load_preset(const SubGhzDevice* d, int p, uint8_t* r);
void subghz_devices_set_frequency(const SubGhzDevice* d, uint32_t f);
void subghz_devices_set_rx(const SubGhzDevice* d);
bool subghz_devices_set_tx(const SubGhzDevice* d);
float subghz_devices_get_rssi(const SubGhzDevice* d);
void subghz_devices_start_async_rx(const SubGhzDevice* d, void* cb, void* ctx);
void subghz_devices_stop_async_rx(const SubGhzDevice* d);
#define FuriHalSubGhzPresetCustom 0

// subghz protocol chain
typedef enum { SubGhzProtocolFlag_Decodable = 1, SubGhzProtocolFlag_433 = 2 } SubGhzProtocolFlag;
typedef void (*SubGhzReceiverCallback)(SubGhzReceiver*, SubGhzProtocolDecoderBase*, void*);
SubGhzEnvironment* subghz_environment_alloc(void);
void subghz_environment_free(SubGhzEnvironment* e);
void subghz_environment_set_protocol_registry(SubGhzEnvironment* e, const SubGhzProtocolRegistry* r);
SubGhzReceiver* subghz_receiver_alloc_init(SubGhzEnvironment* e);
void subghz_receiver_free(SubGhzReceiver* r);
void subghz_receiver_decode(SubGhzReceiver* r, bool level, uint32_t duration);
void subghz_receiver_reset(SubGhzReceiver* r);
void subghz_receiver_set_filter(SubGhzReceiver* r, SubGhzProtocolFlag f);
void subghz_receiver_set_rx_callback(SubGhzReceiver* r, SubGhzReceiverCallback cb, void* ctx);
bool subghz_protocol_decoder_base_get_string(SubGhzProtocolDecoderBase* b, FuriString* s);
size_t subghz_protocol_registry_count(const SubGhzProtocolRegistry* r);
extern const SubGhzProtocolRegistry subghz_protocol_registry;

// preset register tables the app externs
extern const uint8_t subghz_device_cc1101_preset_ook_650khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_ook_270khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_2fsk_dev47_6khz_async_regs[];
