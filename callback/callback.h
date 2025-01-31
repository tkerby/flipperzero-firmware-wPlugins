#pragma once
#include <flip_world.h>
#include <flip_storage/storage.h>

void free_all_views(void *context, bool should_free_variable_item_list, bool should_free_submenu_settings);
void callback_submenu_choices(void *context, uint32_t index);
uint32_t callback_to_submenu(void *context);

// Add edits by Derek Jamison
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

typedef enum MessageState MessageState;
enum MessageState
{
    MessageStateAbout,
    MessageStateLoading,
};
typedef struct MessageModel MessageModel;
struct MessageModel
{
    MessageState message_state;
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
void generic_switch_to_view(FlipWorldApp *app, char *title, DataLoaderFetch fetcher, DataLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id);

void loader_draw_callback(Canvas *canvas, void *model);

void loader_init(View *view);

void loader_free_model(View *view);
bool custom_event_callback(void *context, uint32_t index);
