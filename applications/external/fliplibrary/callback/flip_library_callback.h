#ifndef FLIP_LIBRARY_CALLBACK_H
#define FLIP_LIBRARY_CALLBACK_H
#include <flip_library.h>
#include <flip_storage/flip_library_storage.h>

#define MAX_TOKENS 512 // Adjust based on expected JSON size

extern uint32_t random_facts_index;
extern bool sent_random_fact_request;
extern bool random_fact_request_success;
extern bool random_fact_request_success_all;
extern char* random_fact;

// Parse JSON to find the "text" key
char* flip_library_parse_random_fact();

char* flip_library_parse_cat_fact();

char* flip_library_parse_dog_fact();

char* flip_library_parse_quote();

char* flip_library_parse_dictionary();

void flip_library_request_error(Canvas* canvas);

void flip_library_draw_fact(char* message, Widget** widget);

// Callback for drawing the main screen
void view_draw_callback_random_facts(Canvas* canvas, void* model);

void view_draw_callback_dictionary_run(Canvas* canvas, void* model);

// Input callback for the view (async input handling)
bool view_input_callback_random_facts(InputEvent* event, void* context);

void callback_submenu_choices(void* context, uint32_t index);

void text_updated_ssid(void* context);

void text_updated_password(void* context);

void text_updated_dictionary(void* context);

uint32_t callback_to_submenu(void* context);

uint32_t callback_to_wifi_settings(void* context);

uint32_t callback_to_random_facts(void* context);

void settings_item_selected(void* context, uint32_t index);

/**
 * @brief Navigation callback for exiting the application
 * @param context The context - unused
 * @return next view id (VIEW_NONE to exit the app)
 */
uint32_t callback_exit_app(void* context);

#endif // FLIP_LIBRARY_CALLBACK_H
