// Portal Putt: a fantasy and sci-fi minigolf course for the Flipper Zero.
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <notification/notification_messages_notes.h>
#include <storage/storage.h>
#include <math.h>
#include <stdio.h>

#include "course.h"
#include "sim.h"

#define TICK_MS       33
#define CHARGE_PERIOD 1.4f // seconds for a full up-and-down of the power meter
#define AIM_STEP      (3.0f * 3.14159265f / 180.0f)
#define AIM_FINE      (0.75f * 3.14159265f / 180.0f)
#define MAX_OVER      4 // strokes over par before the ball is picked up
#define BEST_FILE     APP_DATA_PATH("best.bin")

typedef enum {
    StateTitle,
    StateIntro,
    StateAim,
    StateRolling,
    StateHoled, // sunk, or picked up
    StateLost,
    StatePause,
    StateCard,
    StateEnd,
} State;

typedef enum {
    PauseResume,
    PauseCard,
    PauseRestart,
    PauseQuit,
    PauseCount,
} PauseItem;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    NotificationApp* notifications;
    bool running;

    State state;
    State card_return; // where the scorecard goes back to
    Sim sim;
    uint8_t hole;
    uint8_t strokes[HOLE_COUNT];
    uint8_t stroke; // strokes on the current hole so far
    float aim;
    float power;
    float charge; // seconds since the OK press
    bool charging;
    uint32_t frame;
    uint8_t msg_left;
    char msg[20];
    char sub[24];
    uint8_t flash[MAX_THINGS];
    uint8_t wall_quiet; // ticks until the next wall click is allowed
    uint8_t pause_item;
    uint8_t best; // best round total, 0 = none
    bool new_best;
} App;

// ---------------------------------------------------------------- sounds

static const NotificationSequence seq_tap =
    {&message_note_g4, &message_delay_25, &message_sound_off, NULL};
static const NotificationSequence seq_hit =
    {&message_note_c5, &message_delay_25, &message_sound_off, NULL};
static const NotificationSequence seq_smash = {
    &message_vibro_on,
    &message_note_e5,
    &message_delay_25,
    &message_sound_off,
    &message_vibro_off,
    NULL};
static const NotificationSequence seq_wall =
    {&message_note_c4, &message_delay_10, &message_sound_off, NULL};
static const NotificationSequence seq_bumper = {
    &message_vibro_on,
    &message_note_e6,
    &message_delay_25,
    &message_vibro_off,
    &message_note_g6,
    &message_delay_25,
    &message_sound_off,
    NULL};
static const NotificationSequence seq_portal = {
    &message_blue_255,
    &message_note_c5,
    &message_delay_25,
    &message_note_g5,
    &message_delay_25,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_blue_0,
    NULL};
static const NotificationSequence seq_sunk = {
    &message_green_255,
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_green_0,
    NULL};
static const NotificationSequence seq_lost = {
    &message_red_255,
    &message_vibro_on,
    &message_note_g4,
    &message_delay_50,
    &message_note_e4,
    &message_delay_50,
    &message_vibro_off,
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    &message_red_0,
    NULL};
static const NotificationSequence seq_click =
    {&message_note_c6, &message_delay_10, &message_sound_off, NULL};

static void play(App* app, const NotificationSequence* seq) {
    notification_message(app->notifications, seq);
}

// ---------------------------------------------------------------- helpers

static int course_par(void) {
    int p = 0;
    for(int i = 0; i < HOLE_COUNT; i++)
        p += holes[i].par;
    return p;
}

static int total_strokes(const App* app, int upto) {
    int t = 0;
    for(int i = 0; i < upto; i++)
        t += app->strokes[i];
    return t;
}

static int par_upto(int upto) {
    int t = 0;
    for(int i = 0; i < upto; i++)
        t += holes[i].par;
    return t;
}

static void fmt_rel(char* out, size_t n, int diff) {
    if(diff == 0) {
        snprintf(out, n, "E");
    } else {
        snprintf(out, n, "%+d", diff);
    }
}

static const char* score_term(int strokes, int par) {
    if(strokes == 1) return "HOLE IN ONE!";
    int d = strokes - par;
    switch(d) {
    case -3:
        return "ALBATROSS!";
    case -2:
        return "EAGLE!";
    case -1:
        return "BIRDIE!";
    case 0:
        return "PAR";
    case 1:
        return "BOGEY";
    case 2:
        return "DOUBLE BOGEY";
    case 3:
        return "TRIPLE BOGEY";
    default:
        return d < 0 ? "MIRACLE!" : "OUCH";
    }
}

static void set_msg(App* app, const char* msg, const char* sub, uint8_t ticks) {
    strncpy(app->msg, msg, sizeof(app->msg) - 1);
    app->msg[sizeof(app->msg) - 1] = 0;
    strncpy(app->sub, sub ? sub : "", sizeof(app->sub) - 1);
    app->sub[sizeof(app->sub) - 1] = 0;
    app->msg_left = ticks;
}

static void best_load(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    app->best = 0;
    if(storage_file_open(f, BEST_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t b = 0;
        if(storage_file_read(f, &b, 1) == 1) app->best = b;
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void best_save(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, BEST_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &app->best, 1);
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

// ---------------------------------------------------------------- game flow

static void hole_start(App* app, uint8_t index) {
    app->hole = index;
    app->stroke = 0;
    sim_reset(&app->sim, &holes[index]);
    app->aim = sim_aim_to_cup(&app->sim);
    app->power = 0;
    app->charging = false;
    memset(app->flash, 0, sizeof(app->flash));
    app->state = StateIntro;
}

static void round_start(App* app) {
    memset(app->strokes, 0, sizeof(app->strokes));
    app->new_best = false;
    hole_start(app, 0);
}

static void shoot(App* app) {
    app->charging = false;
    app->stroke++;
    sim_shoot(&app->sim, app->aim, app->power);
    app->state = StateRolling;
    if(app->power < 30) {
        play(app, &seq_tap);
    } else if(app->power < 70) {
        play(app, &seq_hit);
    } else {
        play(app, &seq_smash);
    }
}

static void hole_done(App* app, bool picked_up) {
    app->strokes[app->hole] = app->stroke;
    char sub[24];
    const Hole* h = &holes[app->hole];
    if(picked_up) {
        snprintf(sub, sizeof(sub), "Stroke limit, %d", app->stroke);
        set_msg(app, "PICKED UP", sub, 70);
        play(app, &seq_lost);
    } else {
        snprintf(sub, sizeof(sub), "%d on a par %d", app->stroke, h->par);
        set_msg(app, score_term(app->stroke, h->par), sub, 70);
        play(app, &seq_sunk);
    }
    app->state = StateHoled;
}

static void round_end(App* app) {
    int total = total_strokes(app, HOLE_COUNT);
    if(app->best == 0 || total < app->best) {
        app->best = total;
        app->new_best = true;
        best_save(app);
    }
    app->state = StateEnd;
}

static void next_hole(App* app) {
    if(app->hole + 1 >= HOLE_COUNT) {
        round_end(app);
    } else {
        hole_start(app, app->hole + 1);
    }
}

static void roll_tick(App* app) {
    Sim* s = &app->sim;
    sim_step(s);
    uint8_t ev = s->events;
    if(app->wall_quiet) app->wall_quiet--;
    if(ev & SimEvSunk) {
        hole_done(app, false);
        return;
    }
    if(ev & SimEvLost) {
        app->stroke++;
        set_msg(app, holes[app->hole].lost_msg, "Penalty stroke", 40);
        play(app, &seq_lost);
        app->state = StateLost;
        return;
    }
    if(ev & SimEvPortal) {
        play(app, &seq_portal);
    } else if(ev & SimEvBumper) {
        if(s->hit_thing < MAX_THINGS) app->flash[s->hit_thing] = 5;
        play(app, &seq_bumper);
    } else if((ev & SimEvWall) && app->wall_quiet == 0) {
        float sp = sqrtf(s->vx * s->vx + s->vy * s->vy);
        if(sp > 25.0f) {
            play(app, &seq_wall);
            app->wall_quiet = 4;
        }
    }
    if(ev & SimEvStop) {
        if(app->stroke >= holes[app->hole].par + MAX_OVER) {
            hole_done(app, true);
        } else {
            app->aim = sim_aim_to_cup(s);
            app->power = 0;
            app->state = StateAim;
        }
    }
}

// ---------------------------------------------------------------- drawing

static void draw_chevron(Canvas* c, int x, int y, uint8_t dir) {
    switch(dir & 3) {
    case 0: // right
        canvas_draw_line(c, x - 2, y - 3, x + 1, y);
        canvas_draw_line(c, x + 1, y, x - 2, y + 3);
        break;
    case 2: // left
        canvas_draw_line(c, x + 2, y - 3, x - 1, y);
        canvas_draw_line(c, x - 1, y, x + 2, y + 3);
        break;
    case 1: // down
        canvas_draw_line(c, x - 3, y - 2, x, y + 1);
        canvas_draw_line(c, x, y + 1, x + 3, y - 2);
        break;
    default: // up
        canvas_draw_line(c, x - 3, y + 2, x, y - 1);
        canvas_draw_line(c, x, y - 1, x + 3, y + 2);
        break;
    }
}

static void draw_thing(Canvas* c, const App* app, const Thing* t, uint8_t index) {
    uint32_t f = app->frame;
    switch(t->type) {
    case ThingTree:
        canvas_draw_line(c, t->x - 3, t->y + 2, t->x, t->y - 3);
        canvas_draw_line(c, t->x, t->y - 3, t->x + 3, t->y + 2);
        canvas_draw_line(c, t->x - 3, t->y + 2, t->x + 3, t->y + 2);
        canvas_draw_dot(c, t->x, t->y + 3);
        break;
    case ThingPod:
        canvas_draw_circle(c, t->x, t->y, 3);
        if((f / 8 + index) % 2) canvas_draw_dot(c, t->x, t->y);
        break;
    case ThingBumper:
        if(app->flash[index]) {
            canvas_draw_disc(c, t->x, t->y, t->a + 1);
        } else {
            canvas_draw_circle(c, t->x, t->y, t->a);
            canvas_draw_dot(c, t->x, t->y);
        }
        break;
    case ThingPortal: {
        canvas_draw_circle(c, t->x, t->y, 4);
        float a = f * 0.25f + index;
        canvas_draw_dot(c, t->x + (int)roundf(cosf(a) * 2.5f), t->y + (int)roundf(sinf(a) * 2.5f));
        canvas_draw_dot(c, t->x - (int)roundf(cosf(a) * 2.5f), t->y - (int)roundf(sinf(a) * 2.5f));
        static const int8_t dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
        uint8_t d = t->b & 3;
        canvas_draw_line(
            c, t->x + dx[d] * 5, t->y + dy[d] * 5, t->x + dx[d] * 7, t->y + dy[d] * 7);
        break;
    }
    case ThingWell: {
        canvas_draw_disc(c, t->x, t->y, 2);
        for(int arm = 0; arm < 3; arm++) {
            for(int j = 1; j <= 4; j++) {
                float a = -f * 0.1f + arm * 2.0944f + j * 0.55f;
                float r = j * t->a / 5.0f;
                canvas_draw_dot(
                    c, t->x + (int)roundf(cosf(a) * r), t->y + (int)roundf(sinf(a) * r));
            }
        }
        for(int k = 0; k < 12; k++) {
            if((k + f / 6) % 3) continue;
            float a = k * 0.5236f;
            canvas_draw_dot(
                c, t->x + (int)roundf(cosf(a) * t->a), t->y + (int)roundf(sinf(a) * t->a));
        }
        break;
    }
    case ThingLava:
        for(int y = t->y; y < t->y + t->b; y++) {
            for(int x = t->x; x < t->x + t->a; x++) {
                if(((uint32_t)(x + y * 2 + f / 2)) % 5 < 2) canvas_draw_dot(c, x, y);
            }
        }
        break;
    case ThingSlime:
        for(int y = t->y; y < t->y + t->b; y++) {
            for(int x = t->x; x < t->x + t->a; x++) {
                if((x % 4 == 0 && y % 4 == 0) || ((x + 2) % 4 == 0 && (y + 2) % 4 == 0))
                    canvas_draw_dot(c, x, y);
            }
        }
        break;
    case ThingConveyor: {
        uint8_t d = t->c & 3;
        if(d == 0 || d == 2) {
            int cy = t->y + t->b / 2;
            for(int k = 0; k < t->a / 8 + 1; k++) {
                int off = (int)(k * 8 + (d == 0 ? f : 8 - f % 8)) % t->a;
                int x = t->x + off;
                if(x >= t->x + 2 && x < t->x + t->a - 2) draw_chevron(c, x, cy, d);
            }
        } else {
            int cx = t->x + t->a / 2;
            for(int k = 0; k < t->b / 8 + 1; k++) {
                int off = (int)(k * 8 + (d == 1 ? f : 8 - f % 8)) % t->b;
                int y = t->y + off;
                if(y >= t->y + 2 && y < t->y + t->b - 2) draw_chevron(c, cx, y, d);
            }
        }
        break;
    }
    case ThingMover: {
        Rect r = mover_rect(t, app->sim.tick);
        canvas_draw_box(c, r.x, r.y, r.w, r.h);
        canvas_set_color(c, ColorWhite);
        if(holes[app->hole].theme == ThemeFantasy) {
            // a dragon: two eyes and a toothy line
            canvas_draw_dot(c, r.x + 2, r.y + 2);
            canvas_draw_dot(c, r.x + r.w - 3, r.y + 2);
            for(int x = r.x + 1; x < r.x + r.w - 1; x += 2)
                canvas_draw_dot(c, x, r.y + r.h - 2);
        } else {
            // a drone: one blinking eye
            if((f / 4) % 3) canvas_draw_dot(c, r.x + r.w / 2, r.y + r.h / 2);
        }
        canvas_set_color(c, ColorBlack);
        break;
    }
    case ThingWind: {
        // drifting motes show the pull
        for(int k = 0; k < 7; k++) {
            int sx = (k * 37 + 11) % 120 + 3;
            int sy = (k * 23 + 7) % 50 + 11;
            int x = sx + (int)((int32_t)f * t->x / 6);
            int y = sy + (int)((int32_t)f * t->y / 6);
            x = ((x - FIELD_X0) % 125 + 125) % 125 + FIELD_X0;
            y = ((y - FIELD_Y0) % 52 + 52) % 52 + FIELD_Y0;
            canvas_draw_dot(c, x, y);
        }
        break;
    }
    default:
        break;
    }
}

static void draw_course(Canvas* c, const App* app) {
    const Hole* h = &holes[app->hole];
    canvas_draw_frame(c, 0, 9, 128, 55);

    // theme sprinkles: grass tufts or stars
    for(int k = 0; k < 9; k++) {
        int x = (k * 53 + 17 + app->hole * 7) % 118 + 5;
        int y = (k * 31 + 5 + app->hole * 3) % 46 + 13;
        if(h->theme == ThemeFantasy) {
            canvas_draw_dot(c, x, y);
            canvas_draw_dot(c, x + 2, y);
            canvas_draw_dot(c, x + 1, y - 1);
        } else if((app->frame / 10 + k) % 4) {
            canvas_draw_dot(c, x, y);
        }
    }

    for(uint8_t i = 0; i < h->wall_count; i++) {
        const Rect* r = &h->walls[i];
        canvas_draw_box(c, r->x, r->y, r->w, r->h);
    }
    for(uint8_t i = 0; i < h->thing_count; i++)
        draw_thing(c, app, &h->things[i], i);

    // cup and flag
    canvas_draw_circle(c, h->cup_x, h->cup_y, 3);
    canvas_draw_line(c, h->cup_x, h->cup_y - 3, h->cup_x, h->cup_y - 8);
    canvas_draw_line(c, h->cup_x + 1, h->cup_y - 8, h->cup_x + 4, h->cup_y - 7);
    canvas_draw_line(c, h->cup_x + 1, h->cup_y - 6, h->cup_x + 4, h->cup_y - 7);

    // ball
    int bx = (int)roundf(app->sim.x), by = (int)roundf(app->sim.y);
    if(!app->sim.sunk) canvas_draw_disc(c, bx, by, 2);

    // aim
    if(app->state == StateAim || app->state == StatePause || app->state == StateCard) {
        float len = 10.0f + (app->charging ? app->power * 0.18f : 4.0f);
        float cx = cosf(app->aim), cy = sinf(app->aim);
        for(float d = 4; d < len; d += 2.0f) {
            canvas_draw_dot(c, bx + (int)roundf(cx * d), by + (int)roundf(cy * d));
        }
        int tx = bx + (int)roundf(cx * len), ty = by + (int)roundf(cy * len);
        float a1 = app->aim + 2.6f, a2 = app->aim - 2.6f;
        canvas_draw_line(
            c, tx, ty, tx + (int)roundf(cosf(a1) * 3), ty + (int)roundf(sinf(a1) * 3));
        canvas_draw_line(
            c, tx, ty, tx + (int)roundf(cosf(a2) * 3), ty + (int)roundf(sinf(a2) * 3));
    }
}

static void draw_status(Canvas* c, const App* app) {
    const Hole* h = &holes[app->hole];
    char buf[32];
    canvas_set_font(c, FontSecondary);
    snprintf(buf, sizeof(buf), "%d. %s", app->hole + 1, h->name);
    canvas_draw_str(c, 1, 7, buf);
    if(app->charging) {
        canvas_draw_frame(c, 95, 1, 32, 7);
        canvas_draw_box(c, 96, 2, (int)(app->power * 0.30f), 5);
    } else {
        char rel[8];
        fmt_rel(rel, sizeof(rel), total_strokes(app, app->hole) - par_upto(app->hole));
        snprintf(buf, sizeof(buf), "P%d S%d %s", h->par, app->stroke, rel);
        canvas_draw_str_aligned(c, 127, 7, AlignRight, AlignBottom, buf);
    }
}

static void draw_box(Canvas* c, int x, int y, int w, int h) {
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, x, y, w, h);
    canvas_set_color(c, ColorBlack);
    canvas_draw_rframe(c, x, y, w, h, 2);
}

static void draw_message(Canvas* c, const App* app) {
    draw_box(c, 12, 22, 104, 26);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 26, AlignCenter, AlignTop, app->msg);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 44, AlignCenter, AlignBottom, app->sub);
}

static void draw_intro(Canvas* c, const App* app) {
    const Hole* h = &holes[app->hole];
    char buf[32];
    draw_box(c, 10, 18, 108, 34);
    canvas_set_font(c, FontSecondary);
    snprintf(buf, sizeof(buf), "HOLE %d   PAR %d", app->hole + 1, h->par);
    canvas_draw_str_aligned(c, 64, 21, AlignCenter, AlignTop, buf);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 31, AlignCenter, AlignTop, h->name);
    canvas_set_font(c, FontSecondary);
    if((app->frame / 10) % 2)
        canvas_draw_str_aligned(c, 64, 49, AlignCenter, AlignBottom, "OK to tee off");
}

static void draw_title(Canvas* c, const App* app) {
    uint32_t f = app->frame;
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 4, AlignCenter, AlignTop, "PORTAL PUTT");
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 17, AlignCenter, AlignTop, "nine holes, two worlds");

    // castle on the left
    canvas_draw_box(c, 4, 43, 22, 11);
    for(int x = 4; x < 26; x += 4)
        canvas_draw_box(c, x, 40, 2, 3);
    canvas_draw_box(c, 10, 34, 6, 9);
    canvas_draw_line(c, 10, 33, 13, 29);
    canvas_draw_line(c, 13, 29, 16, 33);
    canvas_draw_line(c, 13, 29, 13, 26);
    canvas_draw_line(c, 13, 26, 17, 27);
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 13, 49, 4, 5);
    canvas_draw_dot(c, 7, 46);
    canvas_draw_dot(c, 22, 46);
    canvas_set_color(c, ColorBlack);

    // saucer on the right
    canvas_draw_circle(c, 108, 42, 4);
    canvas_draw_box(c, 96, 45, 25, 3);
    canvas_draw_line(c, 94, 46, 122, 46);
    for(int x = 98; x < 120; x += 5) {
        if(((x / 5) + f / 5) % 2) canvas_draw_dot(c, x, 49);
    }
    canvas_draw_line(c, 100, 48, 98, 54);
    canvas_draw_line(c, 116, 48, 118, 54);
    canvas_draw_line(c, 108, 48, 108, 55);

    // portals with a ball flying between them
    canvas_draw_circle(c, 40, 50, 5);
    canvas_draw_circle(c, 88, 50, 5);
    int t = f % 70;
    if(t < 50) {
        int x = 46 + t * 36 / 50;
        int y = 50 - (int)(sinf(t / 50.0f * 3.14159f) * 10.0f);
        canvas_draw_disc(c, x, y, 2);
    }

    char buf[32];
    if(app->best) {
        char rel[8];
        fmt_rel(rel, sizeof(rel), app->best - course_par());
        snprintf(buf, sizeof(buf), "OK: tee off   best %s", rel);
    } else {
        snprintf(buf, sizeof(buf), "OK: tee off");
    }
    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, buf);
}

static void draw_card(Canvas* c, const App* app, bool final) {
    canvas_clear(c);
    canvas_set_font(c, FontSecondary);
    char buf[16];
    int played = final ? HOLE_COUNT : app->hole;
    canvas_draw_str(c, 2, 8, "Hole");
    canvas_draw_str(c, 2, 20, "Par");
    canvas_draw_str(c, 2, 32, "You");
    for(int i = 0; i < HOLE_COUNT; i++) {
        int x = 30 + i * 10;
        snprintf(buf, sizeof(buf), "%d", i + 1);
        canvas_draw_str_aligned(c, x, 8, AlignCenter, AlignBottom, buf);
        snprintf(buf, sizeof(buf), "%d", holes[i].par);
        canvas_draw_str_aligned(c, x, 20, AlignCenter, AlignBottom, buf);
        if(i < played) {
            snprintf(buf, sizeof(buf), "%d", app->strokes[i]);
        } else {
            snprintf(buf, sizeof(buf), "-");
        }
        canvas_draw_str_aligned(c, x, 32, AlignCenter, AlignBottom, buf);
    }
    canvas_draw_line(c, 0, 10, 127, 10);
    canvas_draw_line(c, 24, 0, 24, 34);
    canvas_draw_line(c, 0, 34, 127, 34);
    int total = total_strokes(app, played);
    int par = par_upto(played);
    char rel[8];
    fmt_rel(rel, sizeof(rel), total - par);
    char line[40];
    if(final) {
        canvas_set_font(c, FontPrimary);
        snprintf(line, sizeof(line), "%d strokes, %s", total, rel);
        canvas_draw_str_aligned(c, 64, 38, AlignCenter, AlignTop, line);
        canvas_set_font(c, FontSecondary);
        if(app->new_best) {
            canvas_draw_str_aligned(
                c,
                64,
                63,
                AlignCenter,
                AlignBottom,
                (app->frame / 10) % 4 ? "NEW BEST ROUND!" : "");
        } else {
            char b[8];
            fmt_rel(b, sizeof(b), app->best - course_par());
            snprintf(line, sizeof(line), "best round %s   OK: again", b);
            canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, line);
        }
    } else {
        snprintf(line, sizeof(line), "After %d: %d strokes, %s", played, total, rel);
        canvas_draw_str_aligned(c, 64, 46, AlignCenter, AlignBottom, line);
        canvas_draw_str_aligned(c, 64, 62, AlignCenter, AlignBottom, "Back: return");
    }
}

static void draw_pause(Canvas* c, const App* app) {
    static const char* items[PauseCount] = {"Resume", "Scorecard", "Restart hole", "Quit"};
    draw_box(c, 34, 12, 60, 44);
    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < PauseCount; i++) {
        int y = 16 + i * 10;
        if(i == app->pause_item) {
            canvas_draw_box(c, 37, y - 1, 54, 10);
            canvas_set_color(c, ColorWhite);
        }
        canvas_draw_str(c, 40, y + 7, items[i]);
        canvas_set_color(c, ColorBlack);
    }
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    switch(app->state) {
    case StateTitle:
        draw_title(canvas, app);
        break;
    case StateEnd:
        draw_card(canvas, app, true);
        break;
    case StateCard:
        draw_card(canvas, app, false);
        break;
    default:
        draw_course(canvas, app);
        draw_status(canvas, app);
        if(app->state == StateIntro) draw_intro(canvas, app);
        if(app->state == StateHoled || app->state == StateLost) draw_message(canvas, app);
        if(app->state == StatePause) draw_pause(canvas, app);
        break;
    }
    furi_mutex_release(app->mutex);
}

// ---------------------------------------------------------------- input

static void input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* queue = context;
    furi_message_queue_put(queue, event, 0);
}

static void handle_input(App* app, const InputEvent* e) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool press = e->type == InputTypePress;
    bool repeat = e->type == InputTypeRepeat;
    bool release = e->type == InputTypeRelease;
    switch(app->state) {
    case StateTitle:
        if(press && e->key == InputKeyOk) round_start(app);
        if(press && e->key == InputKeyBack) app->running = false;
        break;
    case StateIntro:
#ifdef PUTT_DEV
        if(press && e->key == InputKeyRight) {
            hole_start(app, (app->hole + 1) % HOLE_COUNT);
            break;
        }
        if(press && e->key == InputKeyUp) {
            for(int i = 0; i < HOLE_COUNT; i++)
                app->strokes[i] = holes[i].par + (i % 3) - 1;
            round_end(app);
            break;
        }
#endif
        if(press && e->key == InputKeyBack) {
            app->pause_item = PauseResume;
            app->state = StatePause;
        } else if(press) {
            app->state = StateAim;
        }
        break;
    case StateAim:
        if(press && e->key == InputKeyBack) {
            app->charging = false;
            app->pause_item = PauseResume;
            app->state = StatePause;
            break;
        }
        if((press || repeat) && e->key == InputKeyLeft)
            app->aim -= repeat ? AIM_STEP : AIM_FINE * 2;
        if((press || repeat) && e->key == InputKeyRight)
            app->aim += repeat ? AIM_STEP : AIM_FINE * 2;
        if((press || repeat) && e->key == InputKeyUp) app->aim -= AIM_FINE;
        if((press || repeat) && e->key == InputKeyDown) app->aim += AIM_FINE;
        if(press && e->key == InputKeyOk) {
            app->charging = true;
            app->charge = 0;
            app->power = 0;
        }
        if(release && e->key == InputKeyOk && app->charging) shoot(app);
        break;
    case StateRolling:
        if(press && e->key == InputKeyBack) {
            app->pause_item = PauseResume;
            app->state = StatePause;
        }
        break;
    case StateHoled:
        if(press) next_hole(app);
        break;
    case StateLost:
        if(press) {
            app->aim = sim_aim_to_cup(&app->sim);
            app->state = StateAim;
        }
        break;
    case StatePause:
        if(!press) break;
        if(e->key == InputKeyUp) {
            app->pause_item = (app->pause_item + PauseCount - 1) % PauseCount;
            play(app, &seq_click);
        } else if(e->key == InputKeyDown) {
            app->pause_item = (app->pause_item + 1) % PauseCount;
            play(app, &seq_click);
        } else if(e->key == InputKeyBack) {
            app->state = app->sim.rolling ? StateRolling : StateAim;
        } else if(e->key == InputKeyOk) {
            switch(app->pause_item) {
            case PauseResume:
                app->state = app->sim.rolling ? StateRolling : StateAim;
                break;
            case PauseCard:
                app->card_return = StatePause;
                app->state = StateCard;
                break;
            case PauseRestart:
                hole_start(app, app->hole);
                break;
            default:
                app->state = StateTitle;
                break;
            }
        }
        break;
    case StateCard:
        if(press) app->state = app->card_return;
        break;
    case StateEnd:
        if(press && e->key == InputKeyOk) round_start(app);
        if(press && e->key == InputKeyBack) app->state = StateTitle;
        break;
    }
    furi_mutex_release(app->mutex);
}

// ---------------------------------------------------------------- main

static bool tick_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

static void tick(App* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->frame++;
    for(int i = 0; i < MAX_THINGS; i++) {
        if(app->flash[i]) app->flash[i]--;
    }
    switch(app->state) {
    case StateAim:
        if(app->charging) {
            app->charge += TICK_MS / 1000.0f;
            float p = fmodf(app->charge, CHARGE_PERIOD) / (CHARGE_PERIOD * 0.5f);
            app->power = 100.0f * (p < 1.0f ? p : 2.0f - p);
        }
        break;
    case StateRolling:
        roll_tick(app);
        break;
    case StateHoled:
        if(app->msg_left && --app->msg_left == 0) next_hole(app);
        break;
    case StateLost:
        if(app->msg_left && --app->msg_left == 0) {
            app->aim = sim_aim_to_cup(&app->sim);
            app->state = StateAim;
        }
        break;
    default:
        break;
    }
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

int32_t portal_putt_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->running = true;
    app->state = StateTitle;
    sim_reset(&app->sim, &holes[0]);
    best_load(app);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    InputEvent event;
    uint32_t next = furi_get_tick();
    while(app->running) {
        uint32_t now = furi_get_tick();
        uint32_t wait = tick_reached(now, next) ? 0 : next - now;
        if(furi_message_queue_get(app->queue, &event, wait) == FuriStatusOk) {
            handle_input(app, &event);
            continue;
        }
        if(!app->running) break;
        next += TICK_MS;
        if(tick_reached(furi_get_tick(), next + 200)) next = furi_get_tick();
        tick(app);
    }

    notification_message(app->notifications, &sequence_reset_rgb);
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
