#pragma once
#include <flip_world.h>

void callback_submenu_choices(void *context, uint32_t index);

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
