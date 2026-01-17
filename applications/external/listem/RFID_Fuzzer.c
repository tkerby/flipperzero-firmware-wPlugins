#include <furi.h>
#include <furi/core/thread.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ===================== TYPES ===================== */

typedef enum {
    ProtocolRFID,
    ProtocolNFC,
    ProtocolIButton,
} ProtocolType;

typedef struct {
    const char* name;
    uint8_t bytes;
    uint64_t max;
    ProtocolType type;
    const uint8_t* prefixes;
    uint8_t prefix_count;
} Protocol;

/* ===================== PREFIX TABLES ===================== */

/* RFID */
static const uint8_t P_EM4100[] = {0x00, 0x01, 0x02, 0x03};
static const uint8_t P_HID[] = {0xA0, 0xB0, 0xC0};
static const uint8_t P_INDALA[] = {0x20, 0x21, 0x22};
static const uint8_t P_IOPROX[] = {0x20, 0x21, 0x22};
static const uint8_t P_PAC[] = {0xE0};
static const uint8_t P_PARADOX[] = {0xA0, 0xC0};
static const uint8_t P_VIKING[] = {0x40, 0x50};
static const uint8_t P_PYRAMID[] = {0x80};
static const uint8_t P_KERI[] = {0x60};
static const uint8_t P_H10301[] = {0xF0, 0xF1};
static const uint8_t P_JABLO[] = {0x02};
static const uint8_t P_ELECTRA[] = {0x01};
static const uint8_t P_IDTECK[] = {0x0A};
static const uint8_t P_GALL[] = {0x20, 0x30};

/* NFC */
static const uint8_t P_MIFARE[] = {0x04, 0x08, 0x12};
static const uint8_t P_UL[] = {0x04};
static const uint8_t P_ICLASS[] = {0xEC};

/* iButton */
static const uint8_t P_DALLAS[] = {0x01, 0x02};

/* ===================== PROTOCOL LIST ===================== */

static const Protocol protocols[] = {
    /* RFID */
    {"EM4100", 5, 0xFFFFFFFFFF, ProtocolRFID, P_EM4100, 4},
    {"HID Prox", 6, 0xFFFFFFFFFFFF, ProtocolRFID, P_HID, 3},
    {"Indala", 4, 0xFFFFFFFF, ProtocolRFID, P_INDALA, 3},
    {"IoProx", 4, 0xFFFFFFFF, ProtocolRFID, P_IOPROX, 3},
    {"PAC/Stanley", 4, 0xFFFFFFFF, ProtocolRFID, P_PAC, 1},
    {"Paradox", 6, 0xFFFFFFFFFFFF, ProtocolRFID, P_PARADOX, 2},
    {"Viking", 4, 0xFFFFFFFF, ProtocolRFID, P_VIKING, 2},
    {"Pyramid", 4, 0xFFFFFFFF, ProtocolRFID, P_PYRAMID, 1},
    {"Keri", 4, 0xFFFFFFFF, ProtocolRFID, P_KERI, 1},
    {"Nexwatch", 8, 0xFFFFFFFFFFFFFFFF, ProtocolRFID, NULL, 0},
    {"H10301", 3, 0xFFFFFF, ProtocolRFID, P_H10301, 2},
    {"Jablotron", 5, 0xFFFFFFFFFF, ProtocolRFID, P_JABLO, 1},
    {"Electra", 8, 0xFFFFFFFFFFFFFFFF, ProtocolRFID, P_ELECTRA, 1},
    {"IDTeck", 8, 0xFFFFFFFFFFFFFFFF, ProtocolRFID, P_IDTECK, 1},
    {"Gallagher", 8, 0xFFFFFFFFFFFFFFFF, ProtocolRFID, P_GALL, 2},

    /* NFC */
    {"MIFARE Classic 1K", 4, 0xFFFFFFFF, ProtocolNFC, P_MIFARE, 3},
    {"MIFARE Classic 4K", 4, 0xFFFFFFFF, ProtocolNFC, P_MIFARE, 3},
    {"MIFARE Ultralight", 7, 0xFFFFFFFFFFFFFF, ProtocolNFC, P_UL, 1},
    {"DESFire EV1", 16, 0, ProtocolNFC, NULL, 0},
    {"iCLASS", 8, 0xFFFFFFFFFFFFFFFF, ProtocolNFC, P_ICLASS, 1},
    {"FeliCa", 8, 0xFFFFFFFFFFFFFFFF, ProtocolNFC, NULL, 0},

    /* iButton */
    {"Dallas DS1990", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, P_DALLAS, 2},
    {"Cyfral", 2, 0xFFFF, ProtocolIButton, NULL, 0},
    {"Metacom", 4, 0xFFFFFFFF, ProtocolIButton, NULL, 0},
    {"Maxim iButton", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, NULL, 0},
    {"Keypad/Access Control", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, NULL, 0},
    {"Temperature", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, NULL, 0},
    {"Custom iButton", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, NULL, 0},
};

#define PROTOCOL_COUNT (sizeof(protocols) / sizeof(Protocol))
#define MAX_PREFIXES   4

/* ===================== STATE ===================== */

typedef struct {
    size_t proto;
    uint32_t count;
    bool run;

    bool selecting_prefix;
    uint8_t prefix_cursor;
    bool prefix_enabled[PROTOCOL_COUNT][MAX_PREFIXES];
    bool prefix_snapshot[MAX_PREFIXES];

    bool generating;
    uint8_t progress;

    bool list_generated;
    uint32_t msg_timeout;

    bool splash;
    uint32_t splash_timeout;

    FuriThread* worker;
} AppState;

/* ===================== PATH HELPERS ===================== */

static const char* base_path(ProtocolType t) {
    if(t == ProtocolRFID) return "/ext/lfrfid_fuzzer/ListEM";
    if(t == ProtocolNFC) return "/ext/mifare_fuzzer/ListEM";
    return "/ext/ibutton_fuzzer/ListEM";
}

static void ensure_dirs(Storage* st, ProtocolType t) {
    storage_common_mkdir(st, "/ext/lfrfid_fuzzer");
    storage_common_mkdir(st, "/ext/mifare_fuzzer");
    storage_common_mkdir(st, "/ext/ibutton_fuzzer");
    storage_common_mkdir(st, base_path(t));
}

/* ===================== GENERATION ===================== */

static void generate_internal(const Protocol* p, AppState* s) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    ensure_dirs(st, p->type);

    char prefix_part[64] = "random";
    uint8_t used = 0;

    for(uint8_t i = 0; i < p->prefix_count; i++) {
        if(s->prefix_snapshot[i]) {
            if(!used) prefix_part[0] = 0;
            if(used) strcat(prefix_part, "-");
            char tmp[4];
            snprintf(tmp, sizeof(tmp), "%02X", p->prefixes[i]);
            strcat(prefix_part, tmp);
            used++;
        }
    }

    char clean[64];
    strncpy(clean, p->name, sizeof(clean) - 1);
    for(size_t i = 0; clean[i]; i++)
        if(clean[i] == ' ') clean[i] = '_';

    char base[160];
    snprintf(base, sizeof(base), "%s/%s_%s", base_path(p->type), clean, prefix_part);

    char path[192];
    int idx = 0;
    do {
        snprintf(path, sizeof(path), idx ? "%s_%d.txt" : "%s.txt", base, idx);
        idx++;
    } while(storage_common_stat(st, path, NULL) == FSE_OK);

    File* f = storage_file_alloc(st);
    storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    uint32_t step = s->count / 100;
    if(step == 0) step = 1;

    for(uint32_t i = 0; i < s->count; i++) {
        uint64_t id;

        if(p->prefix_count && used > 0) {
            uint8_t usable[MAX_PREFIXES];
            uint8_t n = 0;

            for(uint8_t j = 0; j < p->prefix_count; j++)
                if(s->prefix_snapshot[j]) usable[n++] = p->prefixes[j];

            uint8_t px = usable[rand() % n];
            uint8_t rem = p->bytes - 1;
            uint64_t r = ((uint64_t)rand() << 32) | rand();
            id = ((uint64_t)px << (rem * 8)) | (r & ((1ULL << (rem * 8)) - 1));
        } else {
            uint64_t r = ((uint64_t)rand() << 32) | rand();
            id = p->max ? (r & p->max) : r;
        }

        char line[40];
        snprintf(line, sizeof(line), "%0*llX\n", p->bytes * 2, id);
        storage_file_write(f, line, strlen(line));

        if(i % step == 0) {
            s->progress = (i * 100) / s->count;
            furi_delay_ms(1);
        }
    }

    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

/* ===================== WORKER ===================== */

static int32_t generate_worker(void* ctx) {
    AppState* s = ctx;
    generate_internal(&protocols[s->proto], s);
    s->progress = 100;
    s->generating = false;
    s->list_generated = true;
    s->msg_timeout = furi_get_tick() + 2000;
    return 0;
}

/* ===================== UI ===================== */

static const char* proto_type_str(ProtocolType t) {
    if(t == ProtocolRFID) return "RFID";
    if(t == ProtocolNFC) return "NFC";
    return "iButton";
}

static void draw(Canvas* c, void* ctx) {
    AppState* s = ctx;
    const Protocol* p = &protocols[s->proto];

    canvas_clear(c);

    if(s->splash) {
        canvas_draw_str(c, 28, 30, "ListEM by");
        canvas_draw_str(c, 24, 46, "   CLAWZMAN");
        canvas_draw_str(c, 25, 46, "   CLAWZMAN");
        canvas_draw_str(c, 24, 47, "   CLAWZMAN");
        return;
    }

    if(s->generating) {
        canvas_draw_str(c, 20, 18, "Generating...");
        char buf[8];
        snprintf(buf, sizeof(buf), "%u%%", s->progress);
        canvas_draw_str(c, 52, 32, buf);
        canvas_draw_frame(c, 10, 40, 108, 8);
        canvas_draw_box(c, 11, 41, (s->progress * 106) / 100, 6);
        return;
    }

    if(!s->selecting_prefix) {
        char title[64];
        snprintf(title, sizeof(title), "%s (%s)", p->name, proto_type_str(p->type));

        canvas_draw_str(c, 2, 12, title);
        canvas_draw_str(c, 3, 12, title); /* fake bold */

        char b[32];
        snprintf(b, sizeof(b), "IDs: %lu", s->count);
        canvas_draw_str(c, 2, 26, b);

        canvas_draw_str(c, 2, 38, "Press OK to generate");

        if(p->prefix_count > 0) canvas_draw_str(c, 2, 48, "Hold OK = Prefixes");

        if(s->list_generated) canvas_draw_str(c, 2, 58, "List Generated");
    } else {
        canvas_draw_str(c, 2, 12, "Select Prefixes");

        for(uint8_t i = 0; i < p->prefix_count; i++) {
            char line[32];
            snprintf(
                line,
                sizeof(line),
                "%c %c 0x%02X",
                (i == s->prefix_cursor) ? '>' : ' ',
                s->prefix_enabled[s->proto][i] ? 'X' : ' ',
                p->prefixes[i]);
            canvas_draw_str(c, 2, 24 + i * 10, line);
        }
    }
}

/* ===================== INPUT ===================== */

static void input(InputEvent* e, void* ctx) {
    AppState* s = ctx;
    const Protocol* p = &protocols[s->proto];

    if(s->splash) {
        if(e->type == InputTypeShort && e->key == InputKeyOk) s->splash = false;
        return;
    }

    if(s->generating) return;

    if(e->type == InputTypeLong && e->key == InputKeyOk && p->prefix_count > 0) {
        s->selecting_prefix = true;
        s->prefix_cursor = 0;
        return;
    }

    if(e->type != InputTypeShort) return;

    if(!s->selecting_prefix) {
        if(e->key == InputKeyUp)
            s->proto = (s->proto + 1) % PROTOCOL_COUNT;
        else if(e->key == InputKeyDown)
            s->proto = (s->proto == 0) ? PROTOCOL_COUNT - 1 : s->proto - 1;
        else if(e->key == InputKeyLeft && s->count > 1000)
            s->count -= 1000;
        else if(e->key == InputKeyRight)
            s->count += 1000;
        else if(e->key == InputKeyOk) {
            memcpy(s->prefix_snapshot, s->prefix_enabled[s->proto], sizeof(bool) * MAX_PREFIXES);

            s->generating = true;
            s->progress = 0;

            s->worker = furi_thread_alloc();
            furi_thread_set_name(s->worker, "ListEMGen");
            furi_thread_set_stack_size(s->worker, 4096);
            furi_thread_set_callback(s->worker, generate_worker);
            furi_thread_set_context(s->worker, s);
            furi_thread_start(s->worker);
        } else if(e->key == InputKeyBack)
            s->run = false;
    } else {
        if(e->key == InputKeyUp && s->prefix_cursor > 0)
            s->prefix_cursor--;
        else if(e->key == InputKeyDown && s->prefix_cursor < p->prefix_count - 1)
            s->prefix_cursor++;
        else if(e->key == InputKeyOk)
            s->prefix_enabled[s->proto][s->prefix_cursor] ^= 1;
        else if(e->key == InputKeyBack)
            s->selecting_prefix = false;
    }
}

/* ===================== ENTRY ===================== */

int32_t rfid_fuzzer_app(void* p) {
    UNUSED(p);

    AppState s = {
        .proto = 0,
        .count = 30000,
        .run = true,
        .splash = false,
        .splash_timeout = furi_get_tick() + 2000,
    };

    ViewPort* v = view_port_alloc();
    view_port_draw_callback_set(v, draw, &s);
    view_port_input_callback_set(v, input, &s);

    Gui* g = furi_record_open(RECORD_GUI);
    gui_add_view_port(g, v, GuiLayerFullscreen);

    while(s.run) {
        if(s.splash && furi_get_tick() > s.splash_timeout) s.splash = false;

        if(s.list_generated && furi_get_tick() > s.msg_timeout) s.list_generated = false;

        view_port_update(v);
        furi_delay_ms(50);
    }

    if(s.worker) {
        furi_thread_join(s.worker);
        furi_thread_free(s.worker);
    }

    gui_remove_view_port(g, v);
    view_port_free(v);
    furi_record_close(RECORD_GUI);
    return 0;
}
