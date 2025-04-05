#pragma once
#include <flip_world.h>
#include <flip_storage/storage.h>

void free_all_views(void *context, bool free_variable_list, bool free_settings_submenu, bool free_submenu_game);
void callback_submenu_choices(void *context, uint32_t index);
uint32_t callback_to_submenu(void *context);

typedef enum MessageState MessageState;
enum MessageState
{
    MessageStateAbout,        // The about screen
    MessageStateLoading,      // The loading screen (for game)
    MessageStateWaitingLobby, // The waiting lobby screen
};
typedef struct MessageModel MessageModel;
struct MessageModel
{
    MessageState message_state;
};
