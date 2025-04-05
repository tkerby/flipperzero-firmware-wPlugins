#pragma once
#include <flip_world.h>

// Below added by Derek Jamison
// FURI_LOG_DEV will log only during app development. Be sure that Settings/System/Log Device is "LPUART"; so we dont use serial port.
#ifdef DEVELOPMENT
#define FURI_LOG_DEV(tag, format, ...) furi_log_print_format(FuriLogLevelInfo, tag, format, ##__VA_ARGS__)
#define DEV_CRASH() furi_crash()
#else
#define FURI_LOG_DEV(tag, format, ...)
#define DEV_CRASH()
#endif

typedef enum DataState DataState;
enum DataState
{
    DataStateInitial,
    DataStateRequested,
    DataStateReceived,
    DataStateParsed,
    DataStateParseError,
    DataStateError,
};

typedef struct DataLoaderModel DataLoaderModel;
typedef bool (*DataLoaderFetch)(DataLoaderModel *model);
typedef char *(*DataLoaderParser)(DataLoaderModel *model);
struct DataLoaderModel
{
    char *title;
    char *data_text;
    DataState data_state;
    DataLoaderFetch fetcher;
    DataLoaderParser parser;
    void *parser_context;
    size_t request_index;
    size_t request_count;
    ViewNavigationCallback back_callback;
    FuriTimer *timer;
    FlipperHTTP *fhttp;
};
bool loader_view_alloc(void *context);
void loader_view_free(void *context);
void loader_switch_to_view(FlipWorldApp *app, char *title, DataLoaderFetch fetcher, DataLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id);
void loader_draw_callback(Canvas *canvas, void *model);
void loader_init(View *view);
void loader_widget_set_text(char *message, Widget **widget);
void loader_free_model(View *view);
bool loader_custom_event_callback(void *context, uint32_t index);
