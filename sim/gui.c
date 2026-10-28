// The firmware's stock GUI modules, ported for the host simulator.
//
// Geometry is copied from the firmware source (applications/services/gui/modules/*.c), not
// invented: the menu's three rows sit at baselines 14/36/58 with labels at x=22 and a framed
// middle row at y=21, because that is what menu_draw_callback draws. What is NOT reproduced is
// the icon artwork -- the icons live in a generated asset blob -- so the icon slot is left
// empty and labels keep their real x. That is a visible difference from the device and the
// README says so.
#include <shim.h>

// ---------------------------------------------------------------------------- elements

void elements_frame(Canvas* canvas, uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
    canvas_draw_frame(canvas, x, y, width, height);
}

void elements_scrollbar_pos(Canvas* c, uint8_t x, uint8_t y, uint8_t height, uint16_t pos, uint16_t total) {
    (void)c;
    (void)x;
    (void)y;
    (void)height;
    (void)pos;
    (void)total;
}

void elements_scrollbar(Canvas* c, uint16_t pos, uint16_t total) {
    (void)c;
    (void)pos;
    (void)total;
}

// ---------------------------------------------------------------------------- menu

#define MENU_CAP 24

typedef struct {
    char label[32];
    uint32_t index;
    MenuItemCallback cb;
    void* ctx;
} MenuRow;

struct Menu {
    MenuRow rows[MENU_CAP];
    int n;
    int pos;
    View* view;
    char header[24];
};

static void menu_draw(Canvas* c, void* ctx) {
    Menu* m = ctx;
    canvas_clear(c);
    if(!m->n) {
        canvas_draw_str(c, 2, 32, "Empty");
        return;
    }
    // first line (position-1), cyclic -- exactly as menu_draw_callback does it
    int i0 = (m->pos + m->n - 1) % m->n;
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 22, 14, m->rows[i0].label);

    // middle line, highlighted by a frame from 0,21 to 123,42
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 22, 36, m->rows[m->pos].label);
    elements_frame(c, 0, 21, 128 - 5, 21);

    // third line
    canvas_set_font(c, FontSecondary);
    int i2 = (m->pos + 1) % m->n;
    canvas_draw_str(c, 22, 58, m->rows[i2].label);
}

static bool menu_input(InputEvent* e, void* ctx) {
    Menu* m = ctx;
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return false;
    if(e->key == InputKeyUp) {
        m->pos = (m->pos + m->n - 1) % m->n;
        return true;
    }
    if(e->key == InputKeyDown) {
        m->pos = (m->pos + 1) % m->n;
        return true;
    }
    if(e->key == InputKeyOk && e->type == InputTypeShort) {
        MenuRow* r = &m->rows[m->pos];
        if(r->cb) r->cb(r->ctx, r->index);
        return true;
    }
    return false;
}

Menu* menu_alloc(void) {
    Menu* m = sim_alloc(sizeof(Menu));
    if(!m) return NULL;
    memset(m, 0, sizeof(*m));
    m->view = view_alloc();
    if(!m->view) {
        sim_free(m);
        return NULL;
    }
    view_set_context(m->view, m);
    view_set_draw_callback(m->view, menu_draw);
    view_set_input_callback(m->view, menu_input);
    return m;
}

void menu_free(Menu* m) {
    if(!m) return;
    view_free(m->view);
    sim_free(m);
}

View* menu_get_view(Menu* m) {
    return m ? m->view : NULL;
}

void menu_reset(Menu* m) {
    if(m) m->n = 0;
}

void menu_add_item(Menu* m, const char* label, const Icon* icon, uint32_t index, MenuItemCallback cb, void* ctx) {
    (void)icon;
    if(!m || m->n >= MENU_CAP) return;
    MenuRow* r = &m->rows[m->n++];
    snprintf(r->label, sizeof(r->label), "%s", label ? label : "");
    r->index = index;
    r->cb = cb;
    r->ctx = ctx;
}

void menu_set_selected_item(Menu* m, uint32_t index) {
    if(!m) return;
    for(int i = 0; i < m->n; i++)
        if(m->rows[i].index == index) m->pos = i;
}

uint32_t menu_get_selected_item(Menu* m) {
    return (m && m->n) ? m->rows[m->pos].index : 0;
}

void menu_set_header(Menu* m, const char* header) {
    if(!m) return;
    snprintf(m->header, sizeof(m->header), "%s", header ? header : "");
}

// ---------------------------------------------------------------------------- popup

struct Popup {
    char header[24];
    char text[64];
    View* view;
};

static void popup_draw(Canvas* c, void* ctx) {
    Popup* p = ctx;
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, p->header);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 30, p->text);
}

Popup* popup_alloc(void) {
    Popup* p = sim_alloc(sizeof(Popup));
    if(!p) return NULL;
    memset(p, 0, sizeof(*p));
    p->view = view_alloc();
    if(!p->view) {
        sim_free(p);
        return NULL;
    }
    view_set_context(p->view, p);
    view_set_draw_callback(p->view, popup_draw);
    return p;
}

void popup_free(Popup* p) {
    if(!p) return;
    view_free(p->view);
    sim_free(p);
}

View* popup_get_view(Popup* p) {
    return p ? p->view : NULL;
}

void popup_reset(Popup* p) {
    if(!p) return;
    p->header[0] = p->text[0] = 0;
}

void popup_set_header(Popup* p, const char* text, uint8_t x, uint8_t y, Align h, Align v) {
    (void)x;
    (void)y;
    (void)h;
    (void)v;
    if(!p) return;
    snprintf(p->header, sizeof(p->header), "%s", text ? text : "");
}

void popup_set_text(Popup* p, const char* text, uint8_t x, uint8_t y, Align h, Align v) {
    (void)x;
    (void)y;
    (void)h;
    (void)v;
    if(!p) return;
    snprintf(p->text, sizeof(p->text), "%s", text ? text : "");
}

void popup_set_icon(Popup* p, uint8_t x, uint8_t y, const Icon* icon) {
    (void)p;
    (void)x;
    (void)y;
    (void)icon;
}

void popup_set_timeout(Popup* p, uint32_t timeout) {
    (void)p;
    (void)timeout;
}

void popup_enable_timeout(Popup* p) {
    (void)p;
}

void popup_set_callback(Popup* p, void (*cb)(void*)) {
    (void)p;
    (void)cb;
}

void popup_set_context(Popup* p, void* ctx) {
    (void)p;
    (void)ctx;
}

// ---------------------------------------------------------------------------- widget

#define WIDGET_CAP 16

struct Widget {
    View* view;
    struct {
        char text[40];
        int x, y;
        Font font;
        Align ha;
    } items[WIDGET_CAP];
    int n;
};

static void widget_draw(Canvas* c, void* ctx) {
    Widget* w = ctx;
    canvas_clear(c);
    for(int i = 0; i < w->n; i++) {
        canvas_set_font(c, w->items[i].font);
        canvas_draw_str_aligned(c, w->items[i].x, w->items[i].y, w->items[i].ha, AlignBottom, w->items[i].text);
    }
}

Widget* widget_alloc(void) {
    Widget* w = sim_alloc(sizeof(Widget));
    if(!w) return NULL;
    memset(w, 0, sizeof(*w));
    w->view = view_alloc();
    if(!w->view) {
        sim_free(w);
        return NULL;
    }
    view_set_context(w->view, w);
    view_set_draw_callback(w->view, widget_draw);
    return w;
}

void widget_free(Widget* w) {
    if(!w) return;
    view_free(w->view);
    sim_free(w);
}

View* widget_get_view(Widget* w) {
    return w ? w->view : NULL;
}

void widget_reset(Widget* w) {
    if(w) w->n = 0;
}

void widget_add_string_element(Widget* w, uint8_t x, uint8_t y, Align ha, Align va, Font font, const char* text) {
    (void)va;
    if(!w || w->n >= WIDGET_CAP) return;
    w->items[w->n].x = x;
    w->items[w->n].y = y;
    w->items[w->n].font = font;
    w->items[w->n].ha = ha;
    snprintf(w->items[w->n].text, sizeof(w->items[0].text), "%s", text ? text : "");
    w->n++;
}

void widget_add_text_box_element(Widget* w, uint8_t x, uint8_t y, uint8_t width, uint8_t height, Align ha, Align va, const char* text, bool inverted) {
    (void)width;
    (void)height;
    (void)inverted;
    widget_add_string_element(w, x, (uint8_t)(y + height / 2), ha, va, FontSecondary, text);
}

void widget_add_button_element(Widget* w, InputKey key, const char* text, void* cb, void* ctx) {
    (void)w;
    (void)key;
    (void)text;
    (void)cb;
    (void)ctx;
}

void widget_add_icon_element(Widget* w, uint8_t x, uint8_t y, const Icon* icon) {
    (void)w;
    (void)x;
    (void)y;
    (void)icon;
}

void widget_add_frame_element(Widget* w, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t radius) {
    (void)w;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)radius;
}

void widget_add_xbm_animation(Widget* w, uint8_t x, uint8_t y, void* animation) {
    (void)w;
    (void)x;
    (void)y;
    (void)animation;
}

const char* menu_get_header(Menu* m) {
    return m ? m->header : "";
}

// ---------------------------------------------------------------------------- scene manager
// The real one is a scene stack with enter/exit callbacks. This is the same state machine
// without the view bookkeeping: enough for an app's scene handlers to run and be driven by the
// scripted keys, which is all the simulator needs.
#define SCENE_STACK_MAX 16

struct SceneManager {
    const SceneManagerHandlers* handlers;
    void* context;
    uint32_t stack[SCENE_STACK_MAX];
    uint8_t depth;
    uint32_t states[16];
};

SceneManager* scene_manager_alloc(const SceneManagerHandlers* handlers, void* context) {
    SceneManager* sm = sim_alloc(sizeof(SceneManager));
    if(!sm) return NULL;
    memset(sm, 0, sizeof(*sm));
    sm->handlers = handlers;
    sm->context = context;
    return sm;
}

void scene_manager_free(SceneManager* sm) {
    sim_free(sm);
}

static void scene_enter(SceneManager* sm, uint32_t id) {
    if(id < sm->handlers->scenes_count && sm->handlers->scenes[id].on_enter)
        sm->handlers->scenes[id].on_enter(sm->context);
}

static void scene_exit(SceneManager* sm, uint32_t id) {
    if(id < sm->handlers->scenes_count && sm->handlers->scenes[id].on_exit)
        sm->handlers->scenes[id].on_exit(sm->context);
}

void scene_manager_next_scene(SceneManager* sm, uint32_t id) {
    if(sm->depth) scene_exit(sm, sm->stack[sm->depth - 1]);
    if(sm->depth < SCENE_STACK_MAX) sm->stack[sm->depth++] = id;
    scene_enter(sm, id);
}

bool scene_manager_handle_back_event(SceneManager* sm) {
    if(sm->depth <= 1) return false;
    uint32_t cur = sm->stack[--sm->depth];
    scene_exit(sm, cur);
    scene_enter(sm, sm->stack[sm->depth - 1]);
    return true;
}

void scene_manager_handle_custom_event(SceneManager* sm, uint32_t event) {
    uint32_t cur = sm->depth ? sm->stack[sm->depth - 1] : 0;
    if(cur < sm->handlers->scenes_count && sm->handlers->scenes[cur].on_event) {
        SceneManagerEvent e = {.type = SceneManagerEventTypeCustom, .event = event};
        sm->handlers->scenes[cur].on_event(sm->context, e);
    }
}

void scene_manager_handle_tick_event(SceneManager* sm) {
    uint32_t cur = sm->depth ? sm->stack[sm->depth - 1] : 0;
    if(cur < sm->handlers->scenes_count && sm->handlers->scenes[cur].on_event) {
        SceneManagerEvent e = {.type = SceneManagerEventTypeTick, .event = 0};
        sm->handlers->scenes[cur].on_event(sm->context, e);
    }
}

void scene_manager_set_scene_state(SceneManager* sm, uint32_t id, uint32_t state) {
    if(id < 16) sm->states[id] = state;
}

uint32_t scene_manager_get_scene_state(SceneManager* sm, uint32_t id) {
    return (id < 16) ? sm->states[id] : 0;
}

bool scene_manager_has_previous_scene(SceneManager* sm, uint32_t id) {
    for(uint8_t i = 0; i + 1 < sm->depth; i++)
        if(sm->stack[i] == id) return true;
    return false;
}

bool scene_manager_search_and_switch_to_previous_scene(SceneManager* sm, uint32_t id) {
    for(uint8_t i = 0; i < sm->depth; i++) {
        if(sm->stack[i] == id) {
            while(sm->depth > i + 1) scene_exit(sm, sm->stack[--sm->depth]);
            return true;
        }
    }
    return false;
}

bool scene_manager_search_and_switch_to_another_scene(SceneManager* sm, uint32_t id) {
    return scene_manager_search_and_switch_to_previous_scene(sm, id);
}

void scene_manager_stop(SceneManager* sm) {
    while(sm->depth) scene_exit(sm, sm->stack[--sm->depth]);
}
