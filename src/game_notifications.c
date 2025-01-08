#include "game_notifications.h"

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "game.h"

const NotificationSequence sequence_earn_point = {
    &message_green_255, &message_vibro_on,  &message_note_c7, &message_delay_50,
    &message_sound_off, &message_vibro_off, &message_green_0, NULL,
};

const NotificationSequence sequence_game_over = {
    &message_red_255,
    &message_vibro_on,

    &message_note_ds4,
    &message_delay_10,
    &message_sound_off,
    &message_delay_10,

    &message_note_ds4,
    &message_delay_10,
    &message_sound_off,
    &message_delay_10,

    &message_note_ds4,
    &message_delay_10,
    &message_sound_off,
    &message_delay_10,

    &message_note_ds4,
    &message_delay_10,
    &message_sound_off,
    &message_delay_10,

    &message_note_ds4,
    &message_delay_10,
    &message_sound_off,
    &message_delay_10,

    &message_note_ds4,
    &message_delay_10,
    &message_sound_off,
    &message_delay_10,

    &message_vibro_off,
    &message_red_0,

    NULL,
};

void
game_notify(GameContext* game_context, const NotificationSequence* sequence)
{
    static const NotificationMessage* notification[30];

    size_t input_index = 0;
    size_t result_index = 0;

    for (; (*sequence)[input_index] != NULL; ++input_index) {
        const NotificationMessage* item = (*sequence)[input_index];

        bool is_sound = item->type == NotificationMessageTypeSoundOn ||
                        item->type == NotificationMessageTypeSoundOff;

        bool is_led = item->type == NotificationMessageTypeLedRed ||
                      item->type == NotificationMessageTypeLedGreen ||
                      item->type == NotificationMessageTypeLedBlue;

        bool is_vibro = item->type == NotificationMessageTypeVibro;

        if ((is_sound && game_context->sound == StateOff) ||
            (is_vibro && game_context->vibro == StateOff) ||
            (is_led && game_context->led == StateOff)) {
            continue;
        }

        notification[result_index] = item;
        ++result_index;
    }

    for (size_t index = result_index; index < sizeof(notification); ++index) {
        notification[index] = NULL;
    }

    notification_message(game_context->notification,
                         (const NotificationSequence*)notification);
}
