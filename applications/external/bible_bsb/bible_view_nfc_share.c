#include "bible_view_nfc_share.h"
#include "bible_renderer.h"

#include <string.h>

/* ============================================================================
 * NfcShare layout constants
 * ============================================================================ */
#define NFC_SHARE_HEADER_Y  10
#define NFC_SHARE_DIVIDER_Y 12
#define NFC_SHARE_PROMPT_Y  24
#define NFC_SHARE_PREVIEW_Y 38
#define NFC_SHARE_CANCEL_Y  62

/* ============================================================================
 * NfcShare view — draw callback
 * ============================================================================ */
void bible_bsb_view_nfc_share_draw(Canvas* canvas, void* ctx) {
    BibleApp* app = bible_app_from_draw(ctx);
    BibleAppState* state = app->state;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    /* Header: "NFC Share" or "Export NFC" */
    if(state->nfc_is_export) {
        canvas_draw_str(canvas, BIBLE_MARGIN_X, NFC_SHARE_HEADER_Y, "Export NFC");
    } else {
        canvas_draw_str(canvas, BIBLE_MARGIN_X, NFC_SHARE_HEADER_Y, "NFC Share");
    }
    canvas_draw_line(canvas, 0, NFC_SHARE_DIVIDER_Y, BIBLE_SCREEN_WIDTH, NFC_SHARE_DIVIDER_Y);

    /* "Hold near reader..." prompt */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, BIBLE_MARGIN_X, NFC_SHARE_PROMPT_Y, "Hold near reader...");

    /* Animated pulsing dots — cycle every 10 frames */
    uint8_t pulse = (state->nfc_pulse_counter / 10) % 4;
    state->nfc_pulse_counter++;
    char dots[8] = {0};
    uint8_t d = 0;
    for(uint8_t i = 0; i <= pulse && d < (sizeof(dots) - 1); i++) {
        dots[d++] = '.';
    }
    dots[d] = '\0';
    canvas_draw_str(canvas, 100, NFC_SHARE_PROMPT_Y, dots);

    /* URL preview or "Export JSON" */
    if(state->nfc_url[0] != '\0') {
        canvas_set_font(canvas, FontSecondary);
        char preview[48] = {0};
        if(state->nfc_is_export) {
            strlcpy(preview, "Export JSON", sizeof(preview));
        } else {
            size_t url_len = strlen(state->nfc_url);
            if(url_len > 28) {
                strlcpy(preview, state->nfc_url, 29); /* copy first 28 chars */
                strlcat(preview, "...", sizeof(preview));
            } else {
                strlcpy(preview, state->nfc_url, sizeof(preview));
            }
        }
        canvas_draw_str(canvas, BIBLE_MARGIN_X, NFC_SHARE_PREVIEW_Y, preview);
    }

    /* Bottom hint: "Back to cancel" */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, BIBLE_MARGIN_X, NFC_SHARE_CANCEL_Y, "Back to cancel");
}

/* ============================================================================
 * NfcShare view — input callback
 * ============================================================================ */
bool bible_bsb_view_nfc_share_input(InputEvent* event, void* ctx) {
    BibleApp* app = ctx;

    /* Only process Press, Repeat, Short, Long. Ignore Release.
     * Back is left unconsumed so the NFC share scene can stop emulation. */
    if(event->type != InputTypePress && event->type != InputTypeRepeat &&
       event->type != InputTypeShort && event->type != InputTypeLong) {
        return false;
    }

    bible_app_request_redraw(app);
    return false;
}
