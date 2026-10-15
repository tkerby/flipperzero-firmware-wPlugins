/*
 * nfc_bridge_app.c - NFC USB Bridge: UI + the two worker threads.
 *
 *   Mode 1  PN532 serial : USB CDC (2nd virtual COM port) speaking the PN532 host protocol
 *   Mode 2  PC/SC reader : USB CCID device
 *
 * The protocol engines live in core/ (portable C, tested on a PC). This file only moves
 * bytes between USB and those engines.
 *
 * The card they talk to is either the real NFC hardware (nb_backend_flipper.c) or a "virtual
 * card" loaded from a dump / .nfc file (core/nb_virtual.c): pick "Load dump / .nfc file" in the
 * menu and the PC sees that card as if it were sitting on the reader.
 */
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_cdc.h> /* CdcCallbacks, CDC_DATA_SZ, furi_hal_cdc_* (furi_hal.h does not pull it in) */
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/nb_pn532.h"
#include "core/nb_ccid.h"
#include "core/nb_virtual.h"
#include "nb_backend_flipper.h"
#include "nb_virtual_file.h"
#include "nb_usb_ccid.h"

typedef enum {
    ViewMenu,
    ViewStatus,
} AppView;

typedef enum {
    ModePn532,
    ModeCcid,
} AppMode;

#define FLAG_RX (1u << 0)
#define FLAG_TX (1u << 1)
#define FLAG_STOP (1u << 2)
#define FLAG_ALL (FLAG_RX | FLAG_TX | FLAG_STOP)

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* menu;
    Widget* status;

    AppMode mode;
    FuriThread* worker;
    FbBackend* fb; /* real NFC hardware (only while a mode runs without a loaded file) */
    NbVirtual* virt; /* card loaded from a file, or NULL */
    const NbBackend* be; /* what the running worker talks to: one of the two above */
    FuriHalUsbInterface* prev_usb;

    FuriString* browse_path; /* where the file picker starts */
    char card_path[64]; /* full path of the loaded card, for saving */
    char card_file[32]; /* file name of the loaded card, for the menu */
    char menu_header[48];

    Pn532 pn;
    NbCcid ccid;

    volatile bool usb_up;
} App;

/* ------------------------------------------------------------ PN532 over CDC ch.1 */

static void cdc_rx_cb(void* ctx) {
    furi_thread_flags_set(furi_thread_get_id(((App*)ctx)->worker), FLAG_RX);
}
static void cdc_tx_cb(void* ctx) {
    furi_thread_flags_set(furi_thread_get_id(((App*)ctx)->worker), FLAG_TX);
}
static void cdc_state_cb(void* ctx, uint8_t state) { /* the firmware passes 1 = configured, 0 = not */
    ((App*)ctx)->usb_up = (state != 0);
}

static void pn_write(void* io, const uint8_t* data, size_t len) {
    UNUSED(io);
    while(len) {
        uint16_t n = len > CDC_DATA_SZ ? CDC_DATA_SZ : (uint16_t)len;
        furi_thread_flags_clear(FLAG_TX);
        furi_hal_cdc_send(1, (uint8_t*)data, n);
        /* wait until the host took the packet; give up if the port went away */
        uint32_t f = furi_thread_flags_wait(FLAG_TX | FLAG_STOP, FuriFlagWaitAny, 250);
        if((f & FuriFlagError) == 0 && (f & FLAG_STOP)) return;
        data += n;
        len -= n;
    }
}

static int32_t worker_pn532(void* ctx) {
    App* app = ctx;
    CdcCallbacks cb = {
        .rx_ep_callback = cdc_rx_cb,
        .tx_ep_callback = cdc_tx_cb,
        .state_callback = cdc_state_cb,
    };
    app->prev_usb = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(&usb_cdc_dual, NULL);
    furi_hal_cdc_set_callbacks(1, &cb, app);

    pn532_init(&app->pn, app->be, pn_write, app);

    uint8_t buf[CDC_DATA_SZ];
    for(;;) {
        uint32_t f = furi_thread_flags_wait(FLAG_RX | FLAG_STOP, FuriFlagWaitAny, 200);
        if(!(f & FuriFlagError) && (f & FLAG_STOP)) break;
        int32_t n;
        while((n = furi_hal_cdc_receive(1, buf, sizeof(buf))) > 0) {
            pn532_feed(&app->pn, buf, (size_t)n);
        }
    }

    furi_hal_cdc_set_callbacks(1, NULL, NULL);
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(app->prev_usb ? app->prev_usb : &usb_cdc_single, NULL);
    return 0;
}

/* ------------------------------------------------------------------ CCID worker */

static void ccid_rx_cb(void* ctx) {
    furi_thread_flags_set(furi_thread_get_id(((App*)ctx)->worker), FLAG_RX);
}
static void ccid_tx_cb(void* ctx) {
    furi_thread_flags_set(furi_thread_get_id(((App*)ctx)->worker), FLAG_TX);
}
static void ccid_conn_cb(void* ctx, bool on) {
    ((App*)ctx)->usb_up = on;
}

static void ccid_send(App* app, const uint8_t* data, size_t len) {
    UNUSED(app);
    size_t off = 0;
    bool need_zlp = (len % NB_USB_CCID_PKT) == 0;
    while(off < len) {
        uint16_t n = (len - off) > NB_USB_CCID_PKT ? NB_USB_CCID_PKT : (uint16_t)(len - off);
        furi_thread_flags_clear(FLAG_TX);
        if(nb_usb_ccid_write(data + off, n) < 0) return;
        uint32_t f = furi_thread_flags_wait(FLAG_TX | FLAG_STOP, FuriFlagWaitAny, 250);
        if(!(f & FuriFlagError) && (f & FLAG_STOP)) return;
        off += n;
    }
    if(need_zlp) {
        furi_thread_flags_clear(FLAG_TX);
        nb_usb_ccid_write(NULL, 0);
        furi_thread_flags_wait(FLAG_TX | FLAG_STOP, FuriFlagWaitAny, 250);
    }
}

static int32_t worker_ccid(void* ctx) {
    App* app = ctx;
    static const NbUsbCcidCallbacks cbs = {
        .rx = ccid_rx_cb,
        .tx_done = ccid_tx_cb,
        .connected = ccid_conn_cb,
    };
    if(!nb_usb_ccid_start(&cbs, app)) return -1;

    nb_ccid_init(&app->ccid, app->be, NULL);

    uint8_t msg[NB_CCID_MSG_MAX];
    uint8_t out[NB_CCID_MSG_MAX];
    size_t have = 0, want = 0;

    for(;;) {
        uint32_t f = furi_thread_flags_wait(FLAG_RX | FLAG_STOP, FuriFlagWaitAny, 250);
        if(!(f & FuriFlagError) && (f & FLAG_STOP)) break;

        uint8_t pkt[NB_USB_CCID_PKT];
        int32_t n;
        while((n = nb_usb_ccid_read(pkt, sizeof(pkt))) > 0) {
            if(have + (size_t)n > sizeof(msg)) { /* oversize / desync: drop */
                have = want = 0;
                continue;
            }
            memcpy(msg + have, pkt, (size_t)n);
            have += (size_t)n;
            if(have >= NB_CCID_HDR && want == 0) {
                uint32_t dl = msg[1] | (msg[2] << 8) | (msg[3] << 16) | ((uint32_t)msg[4] << 24);
                want = NB_CCID_HDR + dl;
                if(want > sizeof(msg)) {
                    have = want = 0;
                    continue;
                }
            }
            if(want && have >= want) {
                size_t olen = nb_ccid_process(&app->ccid, msg, want, out, sizeof(out));
                have = want = 0;
                if(olen) ccid_send(app, out, olen);
            }
        }

        /* idle: tell the host when a card appears / disappears */
        if(have == 0) {
            uint8_t note[2];
            if(nb_ccid_poll_notify(&app->ccid, note) == 2) nb_usb_ccid_notify(note, 2);
        }
    }

    nb_usb_ccid_stop();
    return 0;
}

/* ---------------------------------------------------------------------- UI glue */

static App* g_app; /* the widget/submenu views own their view context, so use a global */

static uint32_t clock_ms(void) {
    return furi_get_tick();
}

static void show_status(App* app) {
    widget_reset(app->status);
    const char* title = app->mode == ModePn532 ? "PN532 serial mode" : "PC/SC reader mode";
    widget_add_string_multiline_element(app->status, 64, 4, AlignCenter, AlignTop, FontPrimary, title);

    char text[112];
    if(app->virt) {
        const NbTag* t = nbv_tag(app->virt);
        char uid[2 * NB_UID_MAX + 1];
        for(uint8_t i = 0; i < t->uid_len && i < NB_UID_MAX; i++) snprintf(uid + 2 * i, 3, "%02X", t->uid[i]);
        uid[2 * (t->uid_len < NB_UID_MAX ? t->uid_len : NB_UID_MAX)] = 0;
        bool in = nbv_present(app->virt);
        snprintf(
            text,
            sizeof(text),
            "%s\nUID %s\nCard %s (OK=%s)\nBack = exit",
            nbv_name(app->virt),
            uid,
            in ? "IN" : "OUT",
            in ? "remove" : "insert");
    } else if(app->mode == ModePn532) {
        snprintf(text, sizeof(text), "Use the 2nd COM port\n(\"Flipper\" CDC #2).\nlibnfc: pn532_uart\nBack = exit");
    } else {
        snprintf(text, sizeof(text), "Shows up as a CCID\nsmart-card reader.\npcsc_scan to test\nBack = exit");
    }
    widget_add_string_multiline_element(app->status, 64, 22, AlignCenter, AlignTop, FontSecondary, text);
}

/* OK on the status screen takes the virtual card off the reader / puts it back, so the PC
 * sees a fresh "card inserted" event (pcsc_scan, nfc-poll, ...). */
static bool status_input(InputEvent* event, void* ctx) {
    UNUSED(ctx);
    App* app = g_app;
    if(app->virt && app->worker && event->type == InputTypeShort && event->key == InputKeyOk) {
        nbv_set_present(app->virt, !nbv_present(app->virt));
        show_status(app);
        return true;
    }
    return false; /* Back etc. go to the previous-view callback */
}

static void start_mode(App* app, AppMode mode) {
    app->mode = mode;
    if(app->virt) {
        nbv_set_present(app->virt, true);
        app->be = nbv_backend(app->virt);
    } else {
        app->fb = fb_backend_alloc(); /* takes over the NFC hardware */
        app->be = fb_backend_get(app->fb);
    }
    app->worker = furi_thread_alloc_ex(
        mode == ModePn532 ? "NbPn532" : "NbCcid",
        3 * 1024,
        mode == ModePn532 ? worker_pn532 : worker_ccid,
        app);
    furi_thread_start(app->worker);
    show_status(app);
    view_dispatcher_switch_to_view(app->vd, ViewStatus);
}

static void stop_mode(App* app) {
    if(!app->worker) return;
    furi_thread_flags_set(furi_thread_get_id(app->worker), FLAG_STOP);
    furi_thread_join(app->worker);
    furi_thread_free(app->worker);
    app->worker = NULL;
    app->be = NULL;
    if(app->fb) {
        fb_backend_free(app->fb);
        app->fb = NULL;
    }
}

/* --------------------------------------------------------- load a dump / .nfc file */

static void show_message(const char* header, const char* text) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogMessage* msg = dialog_message_alloc();
    dialog_message_set_header(msg, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(msg, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(msg, NULL, "OK", NULL);
    dialog_message_show(dialogs, msg);
    dialog_message_free(msg);
    furi_record_close(RECORD_DIALOGS);
}

static void rebuild_menu(App* app, uint32_t select);

static void menu_cb(void* ctx, uint32_t index);

static void load_card(App* app) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, "*", NULL); /* .nfc, .mfd, .dump, .bin, ... */
    FuriString* path = furi_string_alloc_set(app->browse_path);
    bool picked = dialog_file_browser_show(dialogs, path, app->browse_path, &options);
    furi_record_close(RECORD_DIALOGS);

    if(picked) {
        furi_string_set(app->browse_path, path);
        NbVirtual* v = nbv_alloc();
        if(!v) {
            show_message("Out of memory", "Cannot load the file");
        } else if(nbv_load_path(v, furi_string_get_cstr(path))) {
            nbv_set_clock(v, clock_ms);
            if(app->virt) nbv_free(app->virt);
            app->virt = v;
            snprintf(app->card_path, sizeof(app->card_path), "%s", furi_string_get_cstr(path));
            FuriString* name = furi_string_alloc();
            path_extract_filename(path, name, false);
            snprintf(app->card_file, sizeof(app->card_file), "%s", furi_string_get_cstr(name));
            furi_string_free(name);
        } else {
            show_message("Cannot use this file", nbv_error(v));
            nbv_free(v);
        }
    }
    furi_string_free(path);
}

static void unload_card(App* app) {
    if(app->virt) {
        if(app->virt->dirty) {
            nbv_save_path(app->virt, app->card_path);
        }
        nbv_free(app->virt);
    }
    app->virt = NULL;
    app->card_file[0] = 0;
    app->card_path[0] = 0;
}

static void rebuild_menu(App* app, uint32_t select) {
    submenu_reset(app->menu);
    if(app->virt) {
        snprintf(app->menu_header, sizeof(app->menu_header), "Card: %s", app->card_file);
    } else {
        snprintf(app->menu_header, sizeof(app->menu_header), "NFC USB Bridge");
    }
    submenu_set_header(app->menu, app->menu_header); /* the menu keeps the pointer */
    submenu_add_item(app->menu, "PN532 serial (libnfc)", 0, menu_cb, app);
    submenu_add_item(app->menu, "PC/SC reader (CCID)", 1, menu_cb, app);
    submenu_add_item(
        app->menu, app->virt ? "Unload file, use real NFC" : "Load dump / .nfc file", 2, menu_cb, app);
    submenu_set_selected_item(app->menu, select);
}

static void menu_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == 2) {
        if(app->virt) {
            unload_card(app);
        } else {
            load_card(app);
        }
        rebuild_menu(app, 2);
        return;
    }
    start_mode(app, index == 0 ? ModePn532 : ModeCcid);
}

static uint32_t prev_status(void* ctx) {
    UNUSED(ctx);
    stop_mode(g_app);
    return ViewMenu;
}

static uint32_t prev_menu(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE; /* exit the app */
}

int32_t nfc_bridge_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    app->menu = submenu_alloc();
    app->status = widget_alloc();

    app->browse_path = furi_string_alloc_set_str(EXT_PATH("nfc"));
    rebuild_menu(app, 0);

    view_dispatcher_add_view(app->vd, ViewMenu, submenu_get_view(app->menu));
    view_dispatcher_add_view(app->vd, ViewStatus, widget_get_view(app->status));
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    g_app = app;
    view_set_previous_callback(widget_get_view(app->status), prev_status);
    view_set_input_callback(widget_get_view(app->status), status_input);
    view_set_previous_callback(submenu_get_view(app->menu), prev_menu);

    view_dispatcher_switch_to_view(app->vd, ViewMenu);
    view_dispatcher_run(app->vd);

    stop_mode(app);
    view_dispatcher_remove_view(app->vd, ViewMenu);
    view_dispatcher_remove_view(app->vd, ViewStatus);
    submenu_free(app->menu);
    widget_free(app->status);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    if(app->virt) nbv_free(app->virt);
    furi_string_free(app->browse_path);
    free(app);
    return 0;
}
