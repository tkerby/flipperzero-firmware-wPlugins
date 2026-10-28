// Host shim: the parts of the Flipper firmware the app touches, with the resources limited the
// way the device limits them.
//
// Two rules:
//   1. Nothing here may be more capable than the device. The heap is a fixed pool of
//      FLIPPER_HEAP bytes with a header per block; when it is exhausted malloc returns NULL,
//      exactly as on the target, so the app's failure paths actually run.
//   2. Anything that would crash the device should crash here. A NULL deref in the sim is a
//      finding, not an annoyance.
//
// The canvas does not rasterise: it emits a draw trace and sim/render.py turns that into a PNG
// with a real font, so layout is judged on real glyph widths rather than an approximation.
#include <shim.h>

#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// ---------------------------------------------------------------------------- logging

void sim_log(char level, const char* tag, const char* fmt, ...) {
    if(level == 'D' && !getenv("SIM_VERBOSE")) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%c] %-10s ", level, tag);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

// ---------------------------------------------------------------------------- heap

#ifndef SIM_HEAP_BYTES
#define SIM_HEAP_BYTES (96u * 1024u) // Flipper has 256 KB SRAM in total; 96 KB is a fair FAP budget
#endif

static struct {
    size_t total;
    size_t used;
    size_t peak;
    size_t blocks;
} g_heap = {SIM_HEAP_BYTES, 0, 0, 0};
static bool g_heap_ready;

// 16-byte header so the accounting survives free() and so a double free is visible
typedef struct {
    uint32_t magic;
    size_t size;
} SimHdr;
#define SIM_MAGIC 0x5A5A5A5A

static void heap_init(void) {
    if(g_heap_ready) return;
    g_heap_ready = true;
    const char* e = getenv("SIM_HEAP");
    if(e) g_heap.total = (size_t)strtoul(e, NULL, 10);
    fprintf(stderr, "[i] heap budget %zu bytes\n", g_heap.total);
}

void* sim_alloc(size_t n) {
    heap_init();
    if(!n) n = 1;
    size_t need = n + sizeof(SimHdr);
    if(g_heap.used + need > g_heap.total) {
        fprintf(stderr, "[!] OOM: asked %zu, %zu of %zu used\n", n, g_heap.used, g_heap.total);
        return NULL;
    }
    SimHdr* h = malloc(need);
    if(!h) return NULL;
    h->magic = SIM_MAGIC;
    h->size = n;
    g_heap.used += need;
    g_heap.blocks++;
    if(g_heap.used > g_heap.peak) g_heap.peak = g_heap.used;
    return (char*)h + sizeof(SimHdr);
}

void sim_free(void* p) {
    if(!p) return;
    SimHdr* h = (SimHdr*)((char*)p - sizeof(SimHdr));
    if(h->magic != SIM_MAGIC) {
        fprintf(stderr, "[!] double free or wild free at %p\n", p);
        abort();
    }
    h->magic = 0;
    g_heap.used -= h->size + sizeof(SimHdr);
    g_heap.blocks--;
    free(h);
}

void sim_heap_report(void) {
    fprintf(stderr, "[i] heap peak %zu / %zu bytes in %zu blocks\n", g_heap.peak, g_heap.total,
            g_heap.blocks);
}

// ---------------------------------------------------------------------------- canvas
// The device's canvas is a thin wrapper over u8g2 (canvas_draw_str == u8g2_DrawUTF8,
// canvas_string_width == u8g2_GetUTF8Width), so the sim does not reimplement drawing: it
// compiles the firmware's own u8g2 and its font tables and renders into a real framebuffer.
// What comes out is pixel-identical to the device, glyphs included.
#include <u8g2.h>

static u8g2_t g_u8g2;
static uint8_t g_fb[128 * 64 / 8];
static Font g_font = FontSecondary;
static Color g_color = ColorBlack;
static char g_scene[64];
static int g_scene_n;
static const char* g_out_dir = "sim/out";

static void scene_write(void) {
    if(!g_scene[0]) return;
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.pgm", g_out_dir, g_scene);
    FILE* f = fopen(path, "wb");
    if(!f) return;
    // u8g2's framebuffer is page-oriented (vertical_top_lsb): one byte per column holds eight
    // vertical pixels, so a 128 px page row is 128 bytes and bit 0 is the TOP pixel.
    fprintf(f, "P5\n%d %d\n255\n", 128, 64);
    for(int y = 0; y < 64; y++) {
        for(int x = 0; x < 128; x++) {
            uint8_t b = g_fb[(y >> 3) * 128 + x];
            fputc((b >> (y & 7)) & 1 ? 0 : 255, f);
        }
    }
    fclose(f);
    fprintf(stderr, "[i] scene %s\n", path);
}

// A 128x64 panel with no hardware behind it. u8g2 still needs the geometry, because
// u8g2_SetupBuffer derives its dimensions from u8x8.display_info.
static const u8x8_display_info_t sim_display_info = {
    .chip_enable_level = 0,
    .chip_disable_level = 1,
    .post_chip_enable_wait_ns = 0,
    .pre_chip_disable_wait_ns = 0,
    .reset_pulse_width_ms = 0,
    .post_reset_wait_ms = 0,
    .sda_setup_time_ns = 0,
    .sck_pulse_width_ns = 0,
    .sck_clock_hz = 1000000,
    .spi_mode = 0,
    .i2c_bus_clock_100kHz = 0,
    .data_setup_time_ns = 0,
    .write_pulse_width_ns = 0,
    .tile_width = 16, // 128 / 8
    .tile_height = 8, // 64 / 8
    .default_x_offset = 0,
    .flipmode_x_offset = 0,
    .pixel_width = 128,
    .pixel_height = 64,
};

void sim_trace_begin(const char* path) {
    if(path && path[0]) g_out_dir = path;
    memset(&g_u8g2, 0, sizeof(g_u8g2));
    g_u8g2.u8x8.display_info = &sim_display_info;
    u8g2_SetupBuffer(&g_u8g2, g_fb, 8, u8g2_ll_hvline_vertical_top_lsb, U8G2_R0);
    u8g2_ClearBuffer(&g_u8g2);
    u8g2_SetFontMode(&g_u8g2, 1); // transparent, same as canvas_set_font
    u8g2_SetDrawColor(&g_u8g2, 1);
    canvas_set_font(NULL, FontSecondary);
}
void sim_trace_end(void) {}

void sim_scene_flush(void);
void sim_scene(const char* name) {
    if(g_scene_n++ == 0) {
        snprintf(g_scene, sizeof(g_scene), "%s", name);
        return;
    }
    scene_write(); // the pixels so far belong to the scene that is ending
    snprintf(g_scene, sizeof(g_scene), "%s", name);
}
void sim_scene_flush(void) {
    scene_write();
}

void canvas_clear(Canvas* c) {
    (void)c;
    u8g2_ClearBuffer(&g_u8g2);
}
void canvas_set_color(Canvas* c, Color col) {
    (void)c;
    g_color = col;
    if(col == ColorXOR)
        u8g2_SetDrawColor(&g_u8g2, 2);
    else
        u8g2_SetDrawColor(&g_u8g2, col == ColorBlack ? 1 : 0);
}
void canvas_set_font(Canvas* c, Font f) {
    (void)c;
    g_font = f;
    // the exact fonts canvas.c picks
    switch(f) {
    case FontPrimary:
        u8g2_SetFont(&g_u8g2, u8g2_font_helvB08_tr);
        break;
    case FontSecondary:
        u8g2_SetFont(&g_u8g2, u8g2_font_haxrcorp4089_tr);
        break;
    case FontKeyboard:
        u8g2_SetFont(&g_u8g2, u8g2_font_profont11_mr);
        break;
    case FontBigNumbers:
        u8g2_SetFont(&g_u8g2, u8g2_font_profont22_tn);
        break;
    default:
        u8g2_SetFont(&g_u8g2, u8g2_font_haxrcorp4089_tr);
        break;
    }
}
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    (void)c;
    u8g2_DrawBox(&g_u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w, (u8g2_uint_t)h);
}
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    (void)c;
    u8g2_DrawFrame(&g_u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w, (u8g2_uint_t)h);
}
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y) {
    (void)c;
    u8g2_DrawPixel(&g_u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y);
}
void canvas_draw_line(Canvas* c, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)c;
    u8g2_DrawLine(
        &g_u8g2, (u8g2_uint_t)x1, (u8g2_uint_t)y1, (u8g2_uint_t)x2, (u8g2_uint_t)y2);
}
void canvas_draw_disc(Canvas* c, int32_t x, int32_t y, size_t r) {
    (void)c;
    u8g2_DrawDisc(&g_u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)r, U8G2_DRAW_ALL);
}
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* s) {
    (void)c;
    u8g2_DrawUTF8(&g_u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, s ? s : "");
}
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align ha, Align va, const char* s) {
    (void)c;
    if(!s) return;
    switch(ha) {
    case AlignRight:
        x -= (int32_t)u8g2_GetUTF8Width(&g_u8g2, s);
        break;
    case AlignCenter:
        x -= (int32_t)(u8g2_GetUTF8Width(&g_u8g2, s) / 2);
        break;
    default:
        break;
    }
    switch(va) {
    case AlignTop:
        y += (int32_t)u8g2_GetAscent(&g_u8g2);
        break;
    case AlignMiddle:
        y += (int32_t)(u8g2_GetAscent(&g_u8g2) / 2);
        break;
    default:
        break;
    }
    u8g2_DrawUTF8(&g_u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, s);
}
size_t canvas_string_width(Canvas* c, const char* s) {
    (void)c;
    if(!s) return 0;
    return u8g2_GetUTF8Width(&g_u8g2, s);
}

// elements_button_* draw an icon plus a frame plus centred text, 12 px tall on the last row.
// The icon is not reproduced (it lives in the firmware's compressed asset blob); the frame and
// the label carry the meaning, and their geometry is the same.
static void pill(int slot, const char* s) {
    const int h = 12, v_off = 3;
    int w = (int)u8g2_GetUTF8Width(&g_u8g2, s) + 10;
    int x = (slot == 0) ? 0 : (slot == 1) ? (128 - w) / 2 : (128 - w);
    u8g2_SetDrawColor(&g_u8g2, 0);
    u8g2_DrawBox(&g_u8g2, (u8g2_uint_t)x, 64 - h - 1, (u8g2_uint_t)w, h);
    u8g2_SetDrawColor(&g_u8g2, 1);
    u8g2_DrawFrame(&g_u8g2, (u8g2_uint_t)x, 64 - h - 1, (u8g2_uint_t)w, h);
    /* the real element inverts its label; on a white-on-black box that means black text */
    u8g2_DrawUTF8(&g_u8g2, (u8g2_uint_t)(x + 5), (u8g2_uint_t)(64 - h - 1 + 9), s);
    (void)v_off;
    u8g2_SetDrawColor(&g_u8g2, 1);
}
void elements_button_left(Canvas* c, const char* s) {
    (void)c;
    pill(0, s);
}
void elements_button_center(Canvas* c, const char* s) {
    (void)c;
    pill(1, s);
}
void elements_button_right(Canvas* c, const char* s) {
    (void)c;
    pill(2, s);
}

// ---------------------------------------------------------------------------- time / sync

static uint64_t g_t0;
uint32_t furi_get_tick(void) {
    if(!g_t0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        g_t0 = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u) - g_t0);
}
void furi_delay_ms(uint32_t ms) {
    if(ms) usleep(ms * 10); // 10x faster than the device: enough sweeps to fill a waterfall
}
void furi_thread_yield(void) {
    sched_yield();
}

// ---------------------------------------------------------------------------- threads
// The requested stack size is honoured as far as the host allows: glibc/macOS refuse stacks
// below PTHREAD_STACK_MIN, so the sim reports the gap instead of pretending. Heap limits are
// exact, stack limits are not -- say so rather than fake it.
struct FuriThread {
    FuriThreadCallback cb;
    void* ctx;
    uint32_t stack;
    pthread_t tid;
    bool started;
};

FuriThread*
    furi_thread_alloc_ex(const char* n, uint32_t stack, FuriThreadCallback cb, void* ctx) {
    (void)n;
    FuriThread* t = sim_alloc(sizeof(FuriThread));
    if(!t) return NULL;
    t->cb = cb;
    t->ctx = ctx;
    t->stack = stack;
    t->started = false;
    return t;
}
void furi_thread_free(FuriThread* t) {
    sim_free(t);
}
static void* thread_tramp(void* arg) {
    FuriThread* t = arg;
    t->cb(t->ctx);
    return NULL;
}
void furi_thread_start(FuriThread* t) {
    if(!t || t->started) return;
    pthread_attr_t a;
    pthread_attr_init(&a);
    size_t want = t->stack ? t->stack : 4096;
    if(want < 16384) {
        static bool warned;
        if(!warned) {
            fprintf(stderr,
                    "[i] note: requested stacks are 3-4 KB, the host minimum is 16 KB; the sim "
                    "runs with 16 KB and cannot reproduce a stack overflow\n");
            warned = true;
        }
        want = 16384;
    }
    pthread_attr_setstacksize(&a, want);
    t->started = true;
    pthread_create(&t->tid, &a, thread_tramp, t);
    pthread_attr_destroy(&a);
}
bool furi_thread_join(FuriThread* t) {
    if(!t || !t->started) return true;
    pthread_join(t->tid, NULL);
    t->started = false;
    return true;
}

struct FuriTimer {
    void (*cb)(void*);
    void* ctx;
};
FuriTimer* furi_timer_alloc(void (*cb)(void*), int type, void* ctx) {
    (void)type;
    FuriTimer* t = sim_alloc(sizeof(FuriTimer));
    if(!t) return NULL;
    t->cb = cb;
    t->ctx = ctx;
    return t;
}
void furi_timer_free(FuriTimer* t) {
    sim_free(t);
}
void furi_timer_start(FuriTimer* t, uint32_t ms) {
    (void)t;
    (void)ms;
} // the driver calls redraw_cb itself
void furi_timer_stop(FuriTimer* t) {
    (void)t;
}

// ---------------------------------------------------------------------------- stream buffer
struct FuriStreamBuffer {
    int32_t* buf;
    size_t cap, head, count;
    size_t trigger;
};
FuriStreamBuffer* furi_stream_buffer_alloc(size_t size, size_t trigger) {
    FuriStreamBuffer* s = sim_alloc(sizeof(FuriStreamBuffer));
    if(!s) return NULL;
    s->cap = size / sizeof(int32_t);
    s->buf = sim_alloc(s->cap * sizeof(int32_t));
    if(!s->buf) {
        sim_free(s);
        return NULL;
    }
    s->head = s->count = 0;
    s->trigger = trigger;
    return s;
}
void furi_stream_buffer_free(FuriStreamBuffer* s) {
    if(!s) return;
    sim_free(s->buf);
    sim_free(s);
}
size_t furi_stream_buffer_send(FuriStreamBuffer* s, const void* data, size_t len, uint32_t t) {
    (void)t;
    if(!s || len != sizeof(int32_t)) return 0;
    if(s->count == s->cap) return 0;
    s->buf[(s->head + s->count) % s->cap] = *(const int32_t*)data;
    s->count++;
    return len;
}
size_t furi_stream_buffer_receive(FuriStreamBuffer* s, void* data, size_t len, uint32_t t) {
    (void)t;
    if(!s || len != sizeof(int32_t) || s->count == 0) return 0;
    *(int32_t*)data = s->buf[s->head];
    s->head = (s->head + 1) % s->cap;
    s->count--;
    return len;
}

// ---------------------------------------------------------------------------- records
static char g_storage, g_gui, g_notif;
void* furi_record_open(const char* n) {
    if(!strcmp(n, RECORD_STORAGE)) return &g_storage;
    if(!strcmp(n, RECORD_GUI)) return &g_gui;
    return &g_notif;
}
void furi_record_close(const char* n) {
    (void)n;
}

// ---------------------------------------------------------------------------- tiny fs
// Enough of a filesystem for the write paths to run: eight files, 8 KB each, in memory.
typedef struct {
    char path[64];
    uint8_t* data;
    size_t len, cap, pos;
    bool used;
} SimFile;
static SimFile g_files[8];
struct File {
    SimFile* f;
};
struct Storage {
    int unused;
};

File* storage_file_alloc(Storage* s) {
    (void)s;
    File* f = sim_alloc(sizeof(File));
    if(f) f->f = NULL;
    return f;
}
void storage_file_free(File* f) {
    sim_free(f);
}
static SimFile* fs_find(const char* p) {
    for(size_t i = 0; i < 8; i++)
        if(g_files[i].used && !strcmp(g_files[i].path, p)) return &g_files[i];
    return NULL;
}
bool storage_file_open(File* f, const char* path, FS_AccessMode m, FS_OpenMode o) {
    if(!f) return false;
    SimFile* sf = fs_find(path);
    if(!sf) {
        if(o == FSOM_OPEN_EXISTING) return false;
        for(size_t i = 0; i < 8; i++)
            if(!g_files[i].used) {
                sf = &g_files[i];
                break;
            }
        if(!sf) return false;
        sf->used = true;
        snprintf(sf->path, sizeof(sf->path), "%s", path);
        sf->cap = 8192;
        sf->data = sim_alloc(sf->cap);
        if(!sf->data) {
            sf->used = false;
            return false;
        }
        sf->len = 0;
    }
    sf->pos = 0;
    f->f = sf;
    (void)m;
    return true;
}
bool storage_file_close(File* f) {
    if(f) f->f = NULL;
    return true;
}
size_t storage_file_read(File* f, void* buf, size_t n) {
    if(!f || !f->f) return 0;
    size_t left = f->f->len - f->f->pos;
    if(n > left) n = left;
    memcpy(buf, f->f->data + f->f->pos, n);
    f->f->pos += n;
    return n;
}
size_t storage_file_write(File* f, const void* buf, size_t n) {
    if(!f || !f->f) return 0;
    SimFile* s = f->f;
    if(s->pos + n > s->cap) n = s->cap - s->pos;
    memcpy(s->data + s->pos, buf, n);
    s->pos += n;
    if(s->pos > s->len) s->len = s->pos;
    return n;
}
bool storage_common_mkdir(Storage* s, const char* p) {
    (void)s;
    (void)p;
    return true;
}

void sim_dump_files(void) {
    for(size_t i = 0; i < 8; i++)
        if(g_files[i].used)
            fprintf(stderr, "[i] file %s (%zu bytes)\n", g_files[i].path, g_files[i].len);
}

// ---------------------------------------------------------------------------- flipper format
// Deliberately minimal: the sim's job is the UI and the resource paths, not the .sub writer.
struct FlipperFormat {
    int unused;
};
FlipperFormat* flipper_format_file_alloc(Storage* s) {
    (void)s;
    FlipperFormat* f = sim_alloc(sizeof(FlipperFormat));
    return f;
}
void flipper_format_free(FlipperFormat* f) {
    sim_free(f);
}
bool flipper_format_file_open_always(FlipperFormat* f, const char* p) {
    (void)f;
    (void)p;
    return false; // forces the app's "can't write" path, which is the interesting one
}
bool flipper_format_file_open_existing(FlipperFormat* f, const char* p) {
    (void)f;
    (void)p;
    return false;
}
bool flipper_format_write_header_cstr(FlipperFormat* f, const char* h, uint32_t v) {
    (void)f;
    (void)h;
    (void)v;
    return false;
}
bool flipper_format_read_header(FlipperFormat* f, FuriString* h, uint32_t* v) {
    (void)f;
    (void)h;
    (void)v;
    return false;
}
bool flipper_format_write_uint32(FlipperFormat* f, const char* k, const uint32_t* d, uint16_t n) {
    (void)f;
    (void)k;
    (void)d;
    (void)n;
    return false;
}
bool flipper_format_write_int32(FlipperFormat* f, const char* k, const int32_t* d, uint16_t n) {
    (void)f;
    (void)k;
    (void)d;
    (void)n;
    return false;
}
bool flipper_format_read_uint32(FlipperFormat* f, const char* k, uint32_t* d, uint16_t n) {
    (void)f;
    (void)k;
    (void)d;
    (void)n;
    return false;
}
bool flipper_format_read_int32(FlipperFormat* f, const char* k, int32_t* d, uint16_t n) {
    (void)f;
    (void)k;
    (void)d;
    (void)n;
    return false;
}
bool flipper_format_write_string_cstr(FlipperFormat* f, const char* k, const char* d) {
    (void)f;
    (void)k;
    (void)d;
    return false;
}
void flipper_format_rewind(FlipperFormat* f) {
    (void)f;
}

// ---------------------------------------------------------------------------- strings
struct FuriString {
    char s[128];
};
FuriString* furi_string_alloc(void) {
    return sim_alloc(sizeof(FuriString));
}
void furi_string_free(FuriString* s) {
    sim_free(s);
}
const char* furi_string_get_cstr(FuriString* s) {
    return s ? s->s : "";
}
int furi_string_cmp_str(FuriString* s, const char* c) {
    return strcmp(furi_string_get_cstr(s), c);
}

// ---------------------------------------------------------------------------- gui / views
struct Canvas {
    int unused;
};
static Canvas g_canvas;

struct View {
    void (*draw)(Canvas*, void*);
    bool (*input)(InputEvent*, void*);
    void* ctx;
};
View* view_alloc(void) {
    View* v = sim_alloc(sizeof(View));
    if(v) memset(v, 0, sizeof(*v));
    return v;
}
void view_free(View* v) {
    sim_free(v);
}
void view_allocate_model(View* v, ViewModelType t, size_t s) {
    (void)v;
    (void)t;
    (void)s;
}
void view_set_context(View* v, void* c) {
    v->ctx = c;
}
void view_set_draw_callback(View* v, void (*cb)(Canvas*, void*)) {
    v->draw = cb;
}
void view_set_input_callback(View* v, bool (*cb)(InputEvent*, void*)) {
    v->input = cb;
}
void view_set_previous_callback(View* v, uint32_t (*cb)(void*)) {
    (void)v;
    (void)cb;
}
void view_set_enter_callback(View* v, void (*cb)(void*)) {
    (void)v;
    (void)cb;
}
void view_set_exit_callback(View* v, void (*cb)(void*)) {
    (void)v;
    (void)cb;
}

struct ViewDispatcher {
    View* views[16];
    uint32_t ids[16];
    uint32_t n;
    uint32_t current;
};
ViewDispatcher* view_dispatcher_alloc(void) {
    ViewDispatcher* vd = sim_alloc(sizeof(ViewDispatcher));
    if(vd) memset(vd, 0, sizeof(*vd));
    return vd;
}
void view_dispatcher_free(ViewDispatcher* vd) {
    sim_free(vd);
}
void view_dispatcher_attach_to_gui(ViewDispatcher* vd, Gui* g, ViewDispatcherType t) {
    (void)vd;
    (void)g;
    (void)t;
}
void view_dispatcher_add_view(ViewDispatcher* vd, uint32_t id, View* v) {
    if(vd->n < 16) {
        vd->ids[vd->n] = id;
        vd->views[vd->n] = v;
        vd->n++;
    }
}
void view_dispatcher_remove_view(ViewDispatcher* vd, uint32_t id) {
    (void)vd;
    (void)id;
}
void view_dispatcher_switch_to_view(ViewDispatcher* vd, uint32_t id) {
    vd->current = id;
}

static View* find_view(ViewDispatcher* vd, uint32_t id) {
    for(uint32_t i = 0; i < vd->n; i++)
        if(vd->ids[i] == id) return vd->views[i];
    return NULL;
}

// The scenario driver. The app calls this once and expects it to block until the user exits;
// here it renders, feeds the same input events a user would, and returns.
// The app blocks here expecting the user; instead render each page and press the same keys a
// person would, so the input handlers are exercised too.
static void sim_press(View* v, InputKey k, InputType t) {
    if(!v || !v->input) return;
    InputEvent e = {.key = k, .type = t};
    v->input(&e, v->ctx);
}

#define SIM_PAGES 7
#define VIEW_SPEC 0

void view_dispatcher_run(ViewDispatcher* vd) {
    view_dispatcher_switch_to_view(vd, VIEW_SPEC);
    View* v = find_view(vd, vd->current);
    if(!v || !v->draw) {
        fprintf(stderr, "[!] spectrum view not registered\n");
        return;
    }
    for(int i = 0; i < 30; i++) usleep(50000); // let the worker fill the buffers

    for(int page = 0; page < SIM_PAGES; page++) {
        char name[24];
        snprintf(name, sizeof(name), "page%d", page);
        sim_scene(name);
        v->draw(&g_canvas, NULL);

        if(page == 5) { // cadence: park, collect, draw that state as well
            sim_press(v, InputKeyOk, InputTypeShort);
            for(int i = 0; i < 16; i++) usleep(50000);
            sim_scene("page5_parked");
            v->draw(&g_canvas, NULL);
            sim_press(v, InputKeyOk, InputTypeShort);
        }
        sim_press(v, InputKeyRight, InputTypeLong); // long = next page
    }
    // Exercise every config row and print index -> label -> action. The row indices are
    // compile-time constants that must match the construction order; getting them wrong makes
    // "Help" start a scan (and, in the lab build, fire the transmitter). This walk makes any
    // future drift visible in one run instead of in the field.
    {
        View* cv = find_view(vd, 2); // VIEW_CONF == 1, but the list view is registered as 1
        (void)cv;
    }
    {
        extern void sim_walk_config(void);
        sim_walk_config();
    }

    // the rest of the app's screens: config list, range editor, capture, help
    static const struct {
        uint32_t id;
        const char* name;
    } other[] = {{1, "config"}, {2, "range"}, {3, "capture"}, {4, "help"}};
    for(size_t i = 0; i < sizeof(other) / sizeof(other[0]); i++) {
        View* ov = find_view(vd, other[i].id);
        if(!ov || !ov->draw) continue;
        sim_scene(other[i].name);
        ov->draw(&g_canvas, ov->ctx);
    }
    sim_scene_flush(); // the last scene never gets a successor to trigger its write
}
View* sim_current_view(ViewDispatcher* vd) {
    return find_view(vd, vd->current);
}
Canvas* sim_canvas(void) {
    return &g_canvas;
}

// ---------------------------------------------------------------------------- variable item list
// Enough of the real widget to judge the menu: five rows, 11 px leading, the selected row
// inverted, label left and value right. The real one also draws a scrollbar and icons.
#define VIL_ROWS 5
struct VariableItem {
    VariableItemList* list;
    int idx;
};
static VariableItemList* g_vil;

struct VariableItemList {
    int n;
    int selected;
    void (*enter)(void*, uint32_t);
    void* enter_ctx;
    char label[24][24];
    char value[24][24];
    View* view;
};

static void vil_draw(Canvas* c, void* ctx) {
    VariableItemList* l = ctx;
    u8g2_ClearBuffer(&g_u8g2);
    u8g2_SetDrawColor(&g_u8g2, 1);
    u8g2_SetFont(&g_u8g2, u8g2_font_haxrcorp4089_tr);
    int first = 0;
    if(l->selected >= VIL_ROWS) first = l->selected - VIL_ROWS + 1;
    for(int row = 0; row < VIL_ROWS && first + row < l->n; row++) {
        int i = first + row;
        int y = row * 11;
        if(i == l->selected) {
            u8g2_SetDrawColor(&g_u8g2, 0);
            u8g2_DrawBox(&g_u8g2, 0, (u8g2_uint_t)y, 128, 11);
            u8g2_SetDrawColor(&g_u8g2, 1);
        }
        u8g2_DrawUTF8(&g_u8g2, 2, (u8g2_uint_t)(y + 9), l->label[i]);
        int w = (int)u8g2_GetUTF8Width(&g_u8g2, l->value[i]);
        u8g2_DrawUTF8(&g_u8g2, (u8g2_uint_t)(126 - w), (u8g2_uint_t)(y + 9), l->value[i]);
    }
    u8g2_SetDrawColor(&g_u8g2, 1);
    (void)c;
}

VariableItemList* variable_item_list_alloc(void) {
    VariableItemList* l = sim_alloc(sizeof(VariableItemList));
    g_vil = l;
    if(!l) return NULL;
    memset(l, 0, sizeof(*l));
    l->view = view_alloc();
    view_set_context(l->view, l);
    view_set_draw_callback(l->view, vil_draw);
    return l;
}
void variable_item_list_free(VariableItemList* l) {
    view_free(l->view);
    sim_free(l);
}
View* variable_item_list_get_view(VariableItemList* l) {
    return l->view;
}
VariableItem* variable_item_list_add(
    VariableItemList* l, const char* lab, uint8_t v, void (*cb)(VariableItem*), void* c) {
    (void)v;
    (void)cb;
    (void)c;
    if(l->n >= 24) return NULL;
    VariableItem* it = sim_alloc(sizeof(VariableItem));
    if(!it) return NULL;
    it->list = l;
    it->idx = l->n;
    snprintf(l->label[l->n], sizeof(l->label[0]), "%s", lab);
    snprintf(l->value[l->n], sizeof(l->value[0]), "%s", " ");
    l->n++;
    return it;
}
void variable_item_list_set_enter_callback(VariableItemList* l, void (*cb)(void*, uint32_t), void* c) {
    l->enter = cb;
    l->enter_ctx = c;
}
void variable_item_set_current_value_index(VariableItem* i, uint8_t idx) {
    (void)i;
    (void)idx;
}
void variable_item_set_current_value_text(VariableItem* i, const char* t) {
    if(!i || !i->list || !t) return;
    snprintf(i->list->value[i->idx], sizeof(i->list->value[0]), "%s", t);
}
uint8_t variable_item_get_current_value_index(VariableItem* i) {
    (void)i;
    return 0;
}
void* variable_item_get_context(VariableItem* i) {
    (void)i;
    return NULL;
}

void sim_walk_config(void) {
    if(!g_vil || !g_vil->enter) return;
    fprintf(stderr, "[i] --- config rows ---\n");
    for(int i = 0; i < g_vil->n; i++) {
        fprintf(stderr, "[i] %2d  %-14s -> ", i, g_vil->label[i]);
        fflush(stderr);
        g_vil->enter(g_vil->enter_ctx, (uint32_t)i);
    }
}

const NotificationSequence sequence_set_vibro_on = {1};
const NotificationSequence sequence_reset_vibro = {2};
void notification_message(NotificationApp* n, const NotificationSequence* s) {
    (void)n;
    (void)s;
}

// ---------------------------------------------------------------------------- radio
// A synthetic spectrum: three carriers of different widths plus a noise floor. Enough for every
// page to have something to draw, deterministic so two runs can be compared.
static uint32_t g_freq = 433920000u;
static uint32_t g_spectrum_seed = 12345u;

void furi_hal_subghz_reset(void) {}
void furi_hal_subghz_idle(void) {}
void furi_hal_subghz_sleep(void) {}
void furi_hal_subghz_set_path(FuriHalSubGhzPath p) {
    (void)p;
}
uint32_t furi_hal_subghz_set_frequency(uint32_t f) {
    g_freq = f;
    return f;
}
uint32_t furi_hal_subghz_set_frequency_and_path(uint32_t f) {
    g_freq = f;
    return f;
}
bool furi_hal_subghz_is_frequency_valid(uint32_t f) {
    return (f >= 300000000u && f <= 348000000u) || (f >= 387000000u && f <= 464000000u) ||
           (f >= 779000000u && f <= 928000000u);
}
void furi_hal_subghz_rx(void) {}
void furi_hal_subghz_load_custom_preset(const uint8_t* r) {
    (void)r;
}
void furi_hal_subghz_start_async_rx(void* cb, void* ctx) {
    (void)cb;
    (void)ctx;
}
void furi_hal_subghz_stop_async_rx(void) {}
bool furi_hal_subghz_start_async_tx(FuriHalSubGhzAsyncTxCallback cb, void* ctx) {
    (void)cb;
    (void)ctx;
    return false;
}
void furi_hal_subghz_stop_async_tx(void) {}
bool furi_hal_subghz_is_async_tx_complete(void) {
    return true;
}

float furi_hal_subghz_get_rssi(void) {
    g_spectrum_seed = g_spectrum_seed * 1103515245u + 12345u;
    int noise = (int)((g_spectrum_seed >> 16) % 13u) - 6; // +-6 dB: a real receiver is not flat
    float r = -98.0f + (float)noise;

    struct {
        uint32_t f;
        int width;
        int level;
    } peaks[] = {
        // inside the default 779-928 MHz range, so the first screen is not empty ...
        {779500000u, 90000, -71},
        {800200000u, 260000, -77}, // a wide, weak carrier
        {826500000u, 40000, -62},
        {855700000u, 60000, -55}, // the strong narrow one in the middle
        {868300000u, 120000, -70},
        {915000000u, 200000, -80},
        {920500000u, 50000, -66},
        // ... and a couple outside it, so switching band shows something new
        {433920000u, 60000, -52},
        {315000000u, 80000, -73},
    };
    for(size_t i = 0; i < sizeof(peaks) / sizeof(peaks[0]); i++) {
        long d = (long)g_freq - (long)peaks[i].f;
        if(d < 0) d = -d;
        if(d < peaks[i].width) {
            int edge = (int)(d * 12 / peaks[i].width); // attenuation toward the skirts
            float v = (float)(peaks[i].level - edge);
            if(v > r) r = v;
        }
    }
    // the jitter applies to the RESULT, not just the floor: a carrier sitting on a peak would
    // otherwise be perfectly flat, and the cadence strip would render as a single line
    r += (float)noise;
    return r;
}

static FuriHalRegion g_region = {"WW", 3, {{299999755, 348000335, 20, 100},
                                           {386999938, 464000000, 20, 100},
                                           {778999847, 928000000, 20, 100}}};
const FuriHalRegion* furi_hal_region_get(void) {
    return &g_region;
}
const char* furi_hal_region_get_name(void) {
    return "WW";
}
void furi_hal_region_set(FuriHalRegion* r) {
    (void)r;
}
bool furi_hal_region_is_frequency_allowed(uint32_t f) {
    return furi_hal_subghz_is_frequency_valid(f);
}

void furi_hal_power_enable_otg(void) {}
void furi_hal_power_disable_otg(void) {}
bool furi_hal_power_is_otg_enabled(void) {
    return false;
}
uint8_t furi_hal_power_get_pct(void) {
    return 64;
}

void furi_hal_rtc_get_datetime(DateTime* d) {
    d->year = 2026;
    d->month = 10;
    d->day = 28;
    d->hour = 14;
    d->minute = 30;
    d->second = 0;
    d->weekday = 3;
}

// The external module is never present, so the app takes its internal-radio path.
const SubGhzDevice* subghz_devices_get_by_name(const char* n) {
    (void)n;
    return NULL;
}
void subghz_devices_init(void) {}
void subghz_devices_deinit(void) {}
bool subghz_devices_is_connect(const SubGhzDevice* d) {
    (void)d;
    return false;
}
void subghz_devices_begin(const SubGhzDevice* d) {
    (void)d;
}
void subghz_devices_end(const SubGhzDevice* d) {
    (void)d;
}
void subghz_devices_reset(const SubGhzDevice* d) {
    (void)d;
}
void subghz_devices_idle(const SubGhzDevice* d) {
    (void)d;
}
void subghz_devices_sleep(const SubGhzDevice* d) {
    (void)d;
}
void subghz_devices_load_preset(const SubGhzDevice* d, int p, uint8_t* r) {
    (void)d;
    (void)p;
    (void)r;
}
void subghz_devices_set_frequency(const SubGhzDevice* d, uint32_t f) {
    (void)d;
    g_freq = f;
}
void subghz_devices_set_rx(const SubGhzDevice* d) {
    (void)d;
}
bool subghz_devices_set_tx(const SubGhzDevice* d) {
    (void)d;
    return false;
}
float subghz_devices_get_rssi(const SubGhzDevice* d) {
    (void)d;
    return furi_hal_subghz_get_rssi();
}
void subghz_devices_start_async_rx(const SubGhzDevice* d, void* cb, void* ctx) {
    (void)d;
    (void)cb;
    (void)ctx;
}
void subghz_devices_stop_async_rx(const SubGhzDevice* d) {
    (void)d;
}

struct SubGhzEnvironment {
    int unused;
};
SubGhzEnvironment* subghz_environment_alloc(void) {
    return sim_alloc(sizeof(SubGhzEnvironment));
}
void subghz_environment_free(SubGhzEnvironment* e) {
    sim_free(e);
}
void subghz_environment_set_protocol_registry(SubGhzEnvironment* e, const SubGhzProtocolRegistry* r) {
    (void)e;
    (void)r;
}
struct SubGhzReceiver {
    int unused;
};
SubGhzReceiver* subghz_receiver_alloc_init(SubGhzEnvironment* e) {
    (void)e;
    return sim_alloc(sizeof(SubGhzReceiver));
}
void subghz_receiver_free(SubGhzReceiver* r) {
    sim_free(r);
}
void subghz_receiver_decode(SubGhzReceiver* r, bool l, uint32_t d) {
    (void)r;
    (void)l;
    (void)d;
}
void subghz_receiver_reset(SubGhzReceiver* r) {
    (void)r;
}
void subghz_receiver_set_filter(SubGhzReceiver* r, SubGhzProtocolFlag f) {
    (void)r;
    (void)f;
}
void subghz_receiver_set_rx_callback(SubGhzReceiver* r, SubGhzReceiverCallback cb, void* c) {
    (void)r;
    (void)cb;
    (void)c;
}
bool subghz_protocol_decoder_base_get_string(SubGhzProtocolDecoderBase* b, FuriString* s) {
    (void)b;
    (void)s;
    return false;
}
size_t subghz_protocol_registry_count(const SubGhzProtocolRegistry* r) {
    (void)r;
    return 0;
}
const SubGhzProtocolRegistry subghz_protocol_registry;

// preset tables: the app only passes the pointer through to a stub, but it must be a real array
const uint8_t subghz_device_cc1101_preset_ook_650khz_async_regs[] = {0x10, 0xF5, 0, 0};
const uint8_t subghz_device_cc1101_preset_ook_270khz_async_regs[] = {0x10, 0xF8, 0, 0};
const uint8_t subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs[] = {0x10, 0xF5, 0, 0};
const uint8_t subghz_device_cc1101_preset_2fsk_dev47_6khz_async_regs[] = {0x10, 0xF5, 0, 0};
