#include "sim.h"
#include <math.h>
#include <stddef.h>

#define FRICTION         55.0f // px/s^2 on the green
#define SLIME_FRICTION   220.0f
#define DAMPING          0.45f // fraction of speed lost per second
#define RESTITUTION      0.78f
#define BUMPER_BOOST     1.25f
#define BUMPER_MIN_SPEED 90.0f
#define CONVEYOR_PUSH    130.0f
#define CUP_PULL         70.0f
#define CUP_MAX_SPEED    125.0f
#define STOP_SPEED       5.0f
#define MAX_ROLL_TICKS   (30 * 14)
#define PORTAL_R         4.5f
#define PORTAL_COOLDOWN  14
#define ROUND_R          3.0f

static const float dir_x[4] = {1, 0, -1, 0};
static const float dir_y[4] = {0, 1, 0, -1};

void sim_reset(Sim* s, const Hole* hole) {
    s->hole = hole;
    s->x = s->shot_x = hole->ball_x;
    s->y = s->shot_y = hole->ball_y;
    s->vx = s->vy = 0;
    s->tick = 0;
    s->roll_ticks = 0;
    s->portal_cooldown = 0;
    s->rolling = false;
    s->sunk = false;
    s->events = 0;
    s->hit_thing = 0xFF;
}

float sim_aim_to_cup(const Sim* s) {
    return atan2f((float)s->hole->cup_y - s->y, (float)s->hole->cup_x - s->x);
}

void sim_shoot(Sim* s, float angle, float power) {
    if(power < 0) power = 0;
    if(power > 100) power = 100;
    float v = POWER_MIN + (POWER_MAX - POWER_MIN) * power / 100.0f;
    s->vx = cosf(angle) * v;
    s->vy = sinf(angle) * v;
    s->shot_x = s->x;
    s->shot_y = s->y;
    s->rolling = true;
    s->roll_ticks = 0;
    s->events = 0;
}

// Push the ball out of a rectangle and reflect its velocity. Returns true on contact.
static bool collide_rect(Sim* s, const Rect* r, float restitution) {
    float cx = s->x, cy = s->y;
    if(cx < r->x) cx = r->x;
    if(cx > r->x + r->w) cx = r->x + r->w;
    if(cy < r->y) cy = r->y;
    if(cy > r->y + r->h) cy = r->y + r->h;
    float dx = s->x - cx, dy = s->y - cy;
    float d2 = dx * dx + dy * dy;
    if(d2 >= BALL_R * BALL_R) return false;
    float nx, ny;
    if(d2 < 0.0001f) {
        // centre is inside: leave through the nearest face
        float left = s->x - r->x, right = r->x + r->w - s->x;
        float top = s->y - r->y, bottom = r->y + r->h - s->y;
        float m = left;
        nx = -1;
        ny = 0;
        if(right < m) {
            m = right;
            nx = 1;
            ny = 0;
        }
        if(top < m) {
            m = top;
            nx = 0;
            ny = -1;
        }
        if(bottom < m) {
            m = bottom;
            nx = 0;
            ny = 1;
        }
        s->x += nx * (m + BALL_R);
        s->y += ny * (m + BALL_R);
    } else {
        float d = sqrtf(d2);
        nx = dx / d;
        ny = dy / d;
        s->x = cx + nx * BALL_R;
        s->y = cy + ny * BALL_R;
    }
    float vn = s->vx * nx + s->vy * ny;
    if(vn < 0) {
        s->vx -= (1.0f + restitution) * vn * nx;
        s->vy -= (1.0f + restitution) * vn * ny;
    }
    return true;
}

static bool collide_round(
    Sim* s,
    float px,
    float py,
    float r,
    float restitution,
    float boost,
    float min_speed) {
    float dx = s->x - px, dy = s->y - py;
    float d2 = dx * dx + dy * dy;
    float rr = r + BALL_R;
    if(d2 >= rr * rr) return false;
    float d = sqrtf(d2);
    float nx, ny;
    if(d < 0.001f) {
        nx = 1;
        ny = 0;
    } else {
        nx = dx / d;
        ny = dy / d;
    }
    s->x = px + nx * rr;
    s->y = py + ny * rr;
    float vn = s->vx * nx + s->vy * ny;
    if(vn < 0) {
        s->vx -= (1.0f + restitution) * vn * nx;
        s->vy -= (1.0f + restitution) * vn * ny;
        if(boost != 1.0f) {
            s->vx *= boost;
            s->vy *= boost;
            float sp = sqrtf(s->vx * s->vx + s->vy * s->vy);
            if(sp < min_speed && sp > 0.01f) {
                s->vx *= min_speed / sp;
                s->vy *= min_speed / sp;
            }
        }
    }
    return true;
}

static void lose_ball(Sim* s) {
    s->x = s->shot_x;
    s->y = s->shot_y;
    s->vx = s->vy = 0;
    s->rolling = false;
    s->events |= SimEvLost | SimEvStop;
}

static void substep(Sim* s, float dt) {
    const Hole* h = s->hole;
    float ax = 0, ay = 0;
    float friction = FRICTION;
    bool on_conveyor = false;

    // field forces
    for(uint8_t i = 0; i < h->thing_count; i++) {
        const Thing* t = &h->things[i];
        switch(t->type) {
        case ThingWind:
            ax += t->x * 4.0f;
            ay += t->y * 4.0f;
            break;
        case ThingWell: {
            float dx = t->x - s->x, dy = t->y - s->y;
            float d = sqrtf(dx * dx + dy * dy);
            if(d < t->a && d > 0.01f) {
                float g = t->b * (1.0f - d / t->a) + 30.0f;
                ax += dx / d * g;
                ay += dy / d * g;
            }
            if(d < 2.5f) {
                lose_ball(s);
                return;
            }
            break;
        }
        case ThingSlime: {
            Rect r = {t->x, t->y, t->a, t->b};
            if(rect_contains(&r, s->x, s->y)) friction = SLIME_FRICTION;
            break;
        }
        case ThingConveyor: {
            Rect r = {t->x, t->y, t->a, t->b};
            if(rect_contains(&r, s->x, s->y)) {
                ax += dir_x[t->c & 3] * CONVEYOR_PUSH;
                ay += dir_y[t->c & 3] * CONVEYOR_PUSH;
                on_conveyor = true;
            }
            break;
        }
        default:
            break;
        }
    }

    // the cup pulls a little when the ball is close
    {
        float dx = h->cup_x - s->x, dy = h->cup_y - s->y;
        float d = sqrtf(dx * dx + dy * dy);
        if(d < 7.0f && d > 0.01f) {
            ax += dx / d * CUP_PULL;
            ay += dy / d * CUP_PULL;
        }
    }

    s->vx += ax * dt;
    s->vy += ay * dt;

    // friction
    float sp = sqrtf(s->vx * s->vx + s->vy * s->vy);
    if(sp > 0.01f) {
        float loss = friction * dt + sp * DAMPING * dt;
        if(loss >= sp) {
            s->vx = s->vy = 0;
            sp = 0;
        } else {
            float k = (sp - loss) / sp;
            s->vx *= k;
            s->vy *= k;
            sp -= loss;
        }
    }

    s->x += s->vx * dt;
    s->y += s->vy * dt;

    // frame
    static const Rect frame[4] = {
        {FIELD_X0 - 20, FIELD_Y0 - 20, 20, 120},
        {FIELD_X1 + 1, FIELD_Y0 - 20, 20, 120},
        {FIELD_X0 - 20, FIELD_Y0 - 20, 180, 20},
        {FIELD_X0 - 20, FIELD_Y1 + 1, 180, 20},
    };
    for(int i = 0; i < 4; i++) {
        if(collide_rect(s, &frame[i], RESTITUTION)) s->events |= SimEvWall;
    }
    for(uint8_t i = 0; i < h->wall_count; i++) {
        if(collide_rect(s, &h->walls[i], RESTITUTION)) s->events |= SimEvWall;
    }

    // things with a body
    for(uint8_t i = 0; i < h->thing_count; i++) {
        const Thing* t = &h->things[i];
        switch(t->type) {
        case ThingTree:
        case ThingPod:
            if(collide_round(s, t->x, t->y, ROUND_R, RESTITUTION, 1.0f, 0)) {
                s->events |= SimEvWall;
                s->hit_thing = i;
            }
            break;
        case ThingBumper:
            if(collide_round(s, t->x, t->y, t->a, 1.0f, BUMPER_BOOST, BUMPER_MIN_SPEED)) {
                s->events |= SimEvBumper;
                s->hit_thing = i;
            }
            break;
        case ThingMover: {
            Rect r = mover_rect(t, s->tick);
            if(collide_rect(s, &r, RESTITUTION)) s->events |= SimEvWall;
            break;
        }
        case ThingLava: {
            Rect r = {t->x, t->y, t->a, t->b};
            if(rect_contains(&r, s->x, s->y)) {
                lose_ball(s);
                return;
            }
            break;
        }
        case ThingPortal:
            if(s->portal_cooldown == 0) {
                float dx = s->x - t->x, dy = s->y - t->y;
                if(dx * dx + dy * dy < PORTAL_R * PORTAL_R) {
                    // find the mate
                    for(uint8_t j = 0; j < h->thing_count; j++) {
                        const Thing* m = &h->things[j];
                        if(j == i || m->type != ThingPortal || m->a != t->a) continue;
                        float v = sqrtf(s->vx * s->vx + s->vy * s->vy);
                        if(v < 60.0f) v = 60.0f;
                        uint8_t d = m->b & 3;
                        s->x = m->x + dir_x[d] * (PORTAL_R + BALL_R + 1.0f);
                        s->y = m->y + dir_y[d] * (PORTAL_R + BALL_R + 1.0f);
                        s->vx = dir_x[d] * v;
                        s->vy = dir_y[d] * v;
                        s->portal_cooldown = PORTAL_COOLDOWN;
                        s->events |= SimEvPortal;
                        break;
                    }
                }
            }
            break;
        default:
            break;
        }
    }

    // cup
    {
        float dx = h->cup_x - s->x, dy = h->cup_y - s->y;
        float d2 = dx * dx + dy * dy;
        if(d2 < CUP_R * CUP_R) {
            float v = sqrtf(s->vx * s->vx + s->vy * s->vy);
            if(v < CUP_MAX_SPEED) {
                s->x = h->cup_x;
                s->y = h->cup_y;
                s->vx = s->vy = 0;
                s->rolling = false;
                s->sunk = true;
                s->events |= SimEvSunk | SimEvStop;
                return;
            }
        }
    }

    sp = sqrtf(s->vx * s->vx + s->vy * s->vy);
    if(sp < STOP_SPEED && !on_conveyor) {
        s->vx = s->vy = 0;
        s->rolling = false;
        s->events |= SimEvStop;
    }
}

void sim_step(Sim* s) {
    s->events = 0;
    s->tick++;
    if(s->portal_cooldown) s->portal_cooldown--;
    if(!s->rolling) return;
    s->roll_ticks++;
    for(int i = 0; i < SIM_SUBSTEPS && s->rolling; i++) {
        substep(s, SIM_DT / SIM_SUBSTEPS);
    }
    if(s->rolling && s->roll_ticks > MAX_ROLL_TICKS) {
        s->vx = s->vy = 0;
        s->rolling = false;
        s->events |= SimEvStop;
    }
}
