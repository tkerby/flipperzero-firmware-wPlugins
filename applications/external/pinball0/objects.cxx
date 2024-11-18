#include <furi.h>
#include <gui/gui.h>

#include "objects.h"
#include "pinball0.h"

Object::Object(const Vec2& p_, float r_)
    : p(p_)
    , prev_p(p_)
    , a({0.0, 0.0})
    , r(r_)
    , physical(true)
    , bounce(1.0f)
    , fixed(false)
    , score(0) {
}

void Object::update(float dt) {
    if(fixed) {
        return;
    }
    Vec2 velocity = p - prev_p;
    // table friction / damping
    velocity *= 0.9999f;
    prev_p = p;
    p = p + velocity + a + (dt * dt);
    a = {0.0, 0.0};
}

void Ball::draw(Canvas* canvas) {
    canvas_draw_disc(canvas, p.x / 10.0f, p.y / 10.0f, r / 10.0f);
}

Flipper::Flipper(const Vec2& p_, Side side_, size_t size_)
    : p(p_)
    , side(side_)
    , size(size_)
    , r(20.0f)
    , max_rotation(1.0f)
    , omega(4.0f)
    , rotation(0.0f)
    , powered(false) {
    if(side_ == Side::LEFT) {
        rest_angle = -0.4f;
        sign = 1;
    } else {
        rest_angle = M_PI + 0.4;
        sign = -1;
    }
}

void Flipper::draw(Canvas* canvas) {
    // base / pivot
    canvas_draw_circle(canvas, p.x / 10, p.y / 10, r / 10);

    // tip
    float angle = rest_angle + sign * rotation;
    Vec2 dir(cos(angle), -sin(angle));

    // draw the tip
    Vec2 tip = p + dir * size;
    canvas_draw_circle(canvas, tip.x / 10, tip.y / 10, r / 10);

    // top and bottom lines
    Vec2 perp(-dir.y, dir.x);
    perp.normalize();
    Vec2 start = p + perp * r;
    Vec2 end = start + dir * size;
    canvas_draw_line(canvas, start.x / 10, start.y / 10, end.x / 10, end.y / 10);

    perp *= -1.0f;
    start = p + perp * r;
    end = start + dir * size;
    canvas_draw_line(canvas, start.x / 10, start.y / 10, end.x / 10, end.y / 10);
}

void Flipper::update(float dt) {
    float prev_rotation = rotation;
    if(powered) {
        rotation = fmin(rotation + dt * omega, max_rotation);
    } else {
        rotation = fmax(rotation - dt * omega, 0.0f);
    }
    current_omega = sign * (rotation - prev_rotation) / dt;
}

bool Flipper::collide(Ball& ball) {
    Vec2 closest = Vec2_closest(p, get_tip(), ball.p);
    Vec2 dir = ball.p - closest;
    float dist = dir.mag();
    if(dist <= VEC2_EPSILON || dist > ball.r + r) {
        return false;
    }
    dir = dir / dist;

    Vec2 ball_v = ball.p - ball.prev_p;

    // adjust ball position
    float corr = ball.r + r - dist;
    ball.p += dir * corr;

    closest += dir * r;
    closest -= p;
    Vec2 perp(-closest.y, closest.x);
    perp *= -1.0f;
    perp.normalize();
    Vec2 surface_velocity = perp * 1.7f; // TODO: flipper power??
    FURI_LOG_I(TAG, "sv: %.3f,%.3f", (double)surface_velocity.x, (double)surface_velocity.y);
    if(current_omega != 0.0f) surface_velocity *= current_omega;
    FURI_LOG_I(TAG, "sv: %.3f,%.3f", (double)surface_velocity.x, (double)surface_velocity.y);

    // TODO: Flippers currently aren't "bouncy" when they are still
    float v = ball_v.dot(dir);
    float v_new = surface_velocity.dot(dir);
    FURI_LOG_I(TAG, "v_new: %.4f, v: %.4f", (double)v_new, (double)v);
    ball_v += dir * (v_new - v);
    ball.prev_p = ball.p - ball_v;
    return true;
}

Vec2 Flipper::get_tip() const {
    float angle = rest_angle + sign * rotation;
    Vec2 dir(cos(angle), -sin(angle));
    Vec2 tip = p + dir * size;
    return tip;
}

void Polygon::draw(Canvas* canvas) {
    if(!hidden) {
        for(size_t i = 0; i < points.size() - 1; i++) {
            canvas_draw_line(
                canvas,
                points[i].x / 10,
                points[i].y / 10,
                points[i + 1].x / 10,
                points[i + 1].y / 10);

#ifdef DRAW_NORMALS
            Vec2 c = (points[i] + points[i + 1]) / 2.0f;
            Vec2 e = c + normals[i] * 40.0f;
            canvas_draw_line(canvas, c.x / 10, c.y / 10, e.x / 10, e.y / 10);
#endif
        }
    }
}

// Attempt to handle double_sided rails better
bool Polygon::collide(Ball& ball) {
    Vec2 ball_v = ball.p - ball.prev_p;
    Vec2 dir;
    Vec2 closest = points[0];
    Vec2 normal = normals[0];
    float min_dist = infinityf();

    for(size_t i = 0; i < points.size() - 1; i++) {
        Vec2& p1 = points[i];
        Vec2& p2 = points[i + 1];

        Vec2 c = Vec2_closest(p1, p2, ball.p);
        dir = ball.p - c;
        float dist = dir.mag();
        if(dist < min_dist) {
            min_dist = dist;
            closest = c;
            normal = normals[i];
        }
    }
    dir = ball.p - closest;
    float dist = dir.mag();
    if(dist > ball.r) {
        return false;
    }

    if(dist <= VEC2_EPSILON) {
        dir = normal;
        dist = normal.mag();
    }
    dir = dir / dist;
    if(dir.dot(normal) >= 0.0f) {
        // FURI_LOG_I(TAG, "Collision Moving TOWARDS");
        ball.p += dir * (ball.r - dist);
    } else {
        // TODO: This is key - we're moving away, so don't alter our v / prev_p!
        // FURI_LOG_I(TAG, "Collision Moving AWAY");
        return false;
        // ball.p += dir * -(dist + ball.r);
    }

    // FURI_LOG_I(
    //     TAG,
    //     "p: %.3f,%.3f  dir: %.3f,%.3f  norm: %.3f,%.3f",
    //     (double)ball.p.x,
    //     (double)ball.p.y,
    //     (double)dir.x,
    //     (double)dir.y,
    //     (double)normal.x,
    //     (double)normal.y);
    float v = ball_v.dot(dir);
    float v_new = fabs(v) * bounce;
    ball_v += dir * (v_new - v);
    ball.prev_p = ball.p - ball_v;
    return true;
}

// Works-ish - 11/5/2024
// bool Polygon::collide(Ball& ball) {
//     Vec2 ball_v = ball.p - ball.prev_p;
//     // We need to check for collisions across all line segments
//     for(size_t i = 0; i < points.size() - 1; i++) {
//         // If ball is moving away from the line, we can't have a collision!
//         if(normals[i].dot(ball_v) > 0) {
//             continue;
//         }

//         Vec2& p1 = points[i];
//         Vec2& p2 = points[i + 1];
//         // bool isLeft_prev = Vec2_ccw(p1, p2, ball.prev_p);
//         // bool isLeft = Vec2_ccw(p1, p2, ball.p);
//         Vec2 closest = Vec2_closest(p1, p2, ball.p);
//         float dist = ball.p.dist(closest);

//         if(dist < ball.r) {
//             // FURI_LOG_I(TAG, "... within collision distance!");
//             // ball_v.dot

//             // float factor = (ball.r - dist) / ball.r;
//             // ball.p -= normals[i] * factor;
//             float depth = ball.r - dist;
//             ball.p -= normals[i] * depth * 1.05f;

//             Vec2 rel_v = ball_v * -1;
//             float velAlongNormal = rel_v.dot(normals[i]);
//             float j = (-(1 + 1) * velAlongNormal);
//             Vec2 impulse = j * normals[i];
//             ball_v -= impulse;

//             ball.prev_p = ball.p - ball_v;
//             return true;
//         }
//     }
//     return false;
// }

void Polygon::finalize() {
    if(points.size() < 2) {
        FURI_LOG_E(TAG, "Polygon: FINALIZE_ERROR - insufficient points");
        return;
    }
    // compute and store normals on all segments
    for(size_t i = 0; i < points.size() - 1; i++) {
        Vec2 normal(points[i + 1].y - points[i].y, points[i].x - points[i + 1].x);
        normal.normalize();
        normals.push_back(normal);
    }
}

void Portal::draw(Canvas* canvas) {
    if(!hidden) {
        Vec2 d;
        Vec2 e;

        // Portal A
        canvas_draw_line(canvas, a1.x / 10, a1.y / 10, a2.x / 10, a2.y / 10);
        d = a1 + au * amag * 0.33f;
        e = d + na * 20.0f;
        canvas_draw_line(canvas, d.x / 10, d.y / 10, e.x / 10, e.y / 10);
        d += au * amag * 0.33f;
        e = d + na * 20.0f;
        canvas_draw_line(canvas, d.x / 10, d.y / 10, e.x / 10, e.y / 10);

        // Portal B
        canvas_draw_line(canvas, b1.x / 10, b1.y / 10, b2.x / 10, b2.y / 10);
        d = b1 + bu * bmag * 0.33f;
        e = d + nb * 20.0f;
        canvas_draw_line(canvas, d.x / 10, d.y / 10, e.x / 10, e.y / 10);
        d += bu * bmag * 0.33f;
        e = d + nb * 20.0f;
        canvas_draw_line(canvas, d.x / 10, d.y / 10, e.x / 10, e.y / 10);

        if(decay > 0) {
            canvas_draw_circle(canvas, enter_p.x / 10, enter_p.y / 10, 2);
        }
    }
#ifdef DRAW_NORMALS
    Vec2 c = (a1 + a2) / 2.0f;
    Vec2 e = c + na * 40.0f;
    canvas_draw_line(canvas, c.x / 10, c.y / 10, e.x / 10, e.y / 10);
    c = (b1 + b2) / 2.0f;
    e = c + nb * 40.0f;
    canvas_draw_line(canvas, c.x / 10, c.y / 10, e.x / 10, e.y / 10);
#endif
}

bool Portal::collide(Ball& ball) {
    Vec2 ball_v = ball.p - ball.prev_p;
    Vec2 dir;
    Vec2 closest;
    Vec2 normal;
    float dist;

    Vec2 a_cl = Vec2_closest(a1, a2, ball.p);
    dir = ball.p - a_cl;
    dist = dir.mag();
    dir = dir / dist;
    if(dist <= ball.r && dir.dot(na) >= 0.0f) {
        // entering portal a! move it to portal b
        // how far "along" the portal are we?
        enter_p = a_cl;
        float offset = (a_cl - a1).mag() / amag;
        ball.p = b2 - bu * (bmag * offset);
        // ensure we're "outside" the next portal to prevent rapid re-entry
        ball.p += nb * ball.r;

        // get projections on entry portal
        float m = -ball_v.dot(au); // tangent magnitude
        float n = ball_v.dot(na); // normal magnitude

        FURI_LOG_I(
            TAG,
            "v: %.3f,%.3f  u: %.3f,%.3f  n: %.3f,%.3f  M: %.3f  N: %.3f",
            (double)ball_v.x,
            (double)ball_v.y,
            (double)au.x,
            (double)au.y,
            (double)na.x,
            (double)na.y,
            (double)m,
            (double)n);

        // transform to exit portal
        ball_v.x = bu.x * m - nb.x * n;
        ball_v.y = bu.y * m - nb.y * n;
        FURI_LOG_I(TAG, "new v: %.3f,%.3f", (double)ball_v.x, (double)ball_v.y);

        ball.prev_p = ball.p - ball_v;
        return true;
    }

    Vec2 b_cl = Vec2_closest(b1, b2, ball.p);
    dir = ball.p - b_cl;
    dist = dir.mag();
    dir = dir / dist;
    if(dist <= ball.r && dir.dot(nb) >= 0.0f) {
        // entering portal b! move it to portal a
        // how far "along" the portal are we?
        enter_p = b_cl;
        float offset = (b_cl - b1).mag() / bmag;
        ball.p = a2 - au * (amag * offset);
        // ensure we're "outside" the next portal to prevent rapid re-entry
        ball.p += na * ball.r;

        // get projections on entry portal
        float m = -ball_v.dot(bu); // tangent magnitude
        float n = ball_v.dot(nb); // normal magnitude

        FURI_LOG_I(
            TAG,
            "v: %.3f,%.3f  u: %.3f,%.3f  n: %.3f,%.3f  M: %.3f  N: %.3f",
            (double)ball_v.x,
            (double)ball_v.y,
            (double)bu.x,
            (double)bu.y,
            (double)nb.x,
            (double)nb.y,
            (double)m,
            (double)n);

        // transform to exit portal
        ball_v.x = au.x * m - na.x * n;
        ball_v.y = au.y * m - na.y * n;
        FURI_LOG_I(TAG, "new v: %.3f,%.3f", (double)ball_v.x, (double)ball_v.y);

        ball.prev_p = ball.p - ball_v;
        return true;
    }
    return false;
}

void Portal::reset_animation() {
    decay = 8;
}
void Portal::step_animation() {
    if(decay > 0) {
        decay--;
    } else {
        decay = 0;
    }
}

void Portal::finalize() {
    na = Vec2(a2.y - a1.y, a1.x - a2.x);
    na.normalize();
    amag = (a2 - a1).mag();
    au = (a2 - a1) / amag;
    nb = Vec2(b2.y - b1.y, b1.x - b2.x);
    nb.normalize();
    bmag = (b2 - b1).mag();
    bu = (b2 - b1) / bmag;
}

Arc::Arc(const Vec2& p_, float r_, float s_, float e_, Surface surf_)
    : FixedObject()
    , p(p_)
    , r(r_)
    , start(s_)
    , end(e_)
    , surface(surf_) {
}

void Arc::draw(Canvas* canvas) {
    if(start == 0 && end == (float)M_PI * 2) {
        canvas_draw_circle(canvas, p.x / 10.0f, p.y / 10.0f, r / 10.0f);
    } else {
        float adj_end = end;
        if(end < start) {
            adj_end += (float)M_PI * 2;
        }
        // initialize to start of arc
        float sx = p.x + r * cosf(start);
        float sy = p.y - r * sinf(start);
        size_t segments = r / 8;
        for(size_t i = 1; i <= segments; i++) { // for now, use r to determin number of segments
            float nx = p.x + r * cosf(start + i / (segments / (adj_end - start)));
            float ny = p.y - r * sinf(start + i / (segments / (adj_end - start)));
            canvas_draw_line(
                canvas, roundf(sx / 10), roundf(sy / 10), roundf(nx / 10), roundf(ny / 10));
            sx = nx;
            sy = ny;
        }
    }
}

// returns value between 0 and 2 PI
// assumes x,y are on cartesean plane, thus you should pass it a neg y
// since the display on flipper is y-inverted
float vector_to_angle(float x, float y) {
    if(x == 0) // special cases UP or DOWN
        return (y > 0) ? M_PI_2 : (y == 0) ? 0 : M_PI + M_PI_2;
    else if(y == 0) // special cases LEFT or RIGHT
        return (x >= 0) ? 0 : M_PI;
    float ret = atanf(y / x); // quadrant I
    if(x < 0 && y < 0) // quadrant III
        ret = (float)M_PI + ret;
    else if(x < 0) // quadrant II
        ret = (float)M_PI + ret; // it actually substracts
    else if(y < 0) // quadrant IV
        ret = (float)M_PI + (float)M_PI_2 + ((float)M_PI_2 + ret); // it actually substracts
    return ret;
}

// Matthias research - 10 minute physics
bool Arc::collide(Ball& ball) {
    Vec2 dir = ball.p - p;
    float dist = dir.mag();
    // FURI_LOG_I(
    //     TAG,
    //     "ball.p: %.3f,%.3f  p: %.3f,%.3f",
    //     (double)ball.p.x,
    //     (double)ball.p.y,
    //     (double)p.x,
    //     (double)p.y);
    // FURI_LOG_I(TAG, "dir: %.3f,%.3f  dist: %.3f", (double)dir.x, (double)dir.y, (double)dist);

    if(surface == OUTSIDE) {
        if(dist > r + ball.r) {
            return false;
        }
        // FURI_LOG_I(TAG, "hitting arc");
        float angle = vector_to_angle(dir.x, -dir.y);
        if((start < end && start <= angle && angle <= end) ||
           (start > end && (angle >= start || angle <= end))) {
            // FURI_LOG_I(TAG, "colliding with arc");
            dir.normalize();

            Vec2 ball_v = ball.p - ball.prev_p;
            float corr = ball.r + r - dist;
            ball.p += dir * corr;
            float v = ball_v.dot(dir);
            ball_v += dir * (3.0f - v); // TODO: pushVel, this should be a prop
            ball.prev_p = ball.p - ball_v;
            return true;
        }
    }
    if(surface == INSIDE) {
        Vec2 prev_dir = ball.prev_p - p;
        float prev_dist = prev_dir.mag();
        if(prev_dist < r && dist + ball.r > r) {
            // FURI_LOG_I(TAG, "Inside an arc!");
            float angle = vector_to_angle(dir.x, -dir.y);
            FURI_LOG_I(TAG, "%f : %f : %f", (double)start, (double)angle, (double)end);
            // if(angle >= start && angle <= end) {
            if((start < end && start <= angle && angle <= end) ||
               (start > end && (angle >= start || angle <= end))) {
                // FURI_LOG_I(TAG, "Within the arc angle");

                dir.normalize();
                Vec2 ball_v = ball.p - ball.prev_p;

                // correct our position to be "on" the arc
                float corr = dist + ball.r - r;
                ball.p -= dir * corr;

                // Adjust restitution on tangent and normals independently
                Vec2 tangent = {-dir.y, dir.x};
                float T = (ball_v.x * tangent.x + ball_v.y * tangent.y) * ARC_TANGENT_RESTITUTION;
                float N = (ball_v.x * tangent.y - ball_v.y * tangent.x) * ARC_NORMAL_RESTITUTION;

                ball_v.x = tangent.x * T - tangent.y * N;
                ball_v.y = tangent.y * T + tangent.x * N;

                // Current collision - works good, but handles restitution holistically
                // float v = ball_v.dot(dir);
                // ball_v -= dir * v * 2.0f * bounce;

                ball.prev_p = ball.p - ball_v;
                return true;
            }
        }
    }
    return false;
}

Bumper::Bumper(const Vec2& p_, float r_)
    : Arc(p_, r_) {
}

void Bumper::draw(Canvas* canvas) {
    Arc::draw(canvas);
    if(decay) {
        canvas_draw_disc(canvas, p.x / 10, p.y / 10, (r / 10) * 0.8f * (decay / 30.0f));
    }
}
void Bumper::reset_animation() {
    decay = 30;
}

void Bumper::step_animation() {
    if(decay > 20) {
        decay--;
    } else {
        decay = 0;
    }
}

void Rollover::draw(Canvas* canvas) {
    if(activated) {
        canvas_draw_str_aligned(canvas, p.x / 10, p.y / 10, AlignCenter, AlignCenter, c);
    } else {
        canvas_draw_dot(canvas, p.x / 10, p.y / 10);
    }
}

bool Rollover::collide(Ball& ball) {
    Vec2 dir = ball.p - p;
    float dist = dir.mag();
    if(dist < 30) {
        activated = true;
    }
    return false;
}

void Turbo::draw(Canvas* canvas) {
    canvas_draw_line(
        canvas, chevron_1[0].x / 10, chevron_1[0].y / 10, chevron_1[1].x / 10, chevron_1[1].y / 10);
    canvas_draw_line(
        canvas, chevron_1[1].x / 10, chevron_1[1].y / 10, chevron_1[2].x / 10, chevron_1[2].y / 10);

    canvas_draw_line(
        canvas, chevron_2[0].x / 10, chevron_2[0].y / 10, chevron_2[1].x / 10, chevron_2[1].y / 10);
    canvas_draw_line(
        canvas, chevron_2[1].x / 10, chevron_2[1].y / 10, chevron_2[2].x / 10, chevron_2[2].y / 10);
}

bool Turbo::collide(Ball& ball) {
    Vec2 dir = ball.p - p;
    float dist = dir.mag();
    if(dist < 30) {
        // apply the turbo in 'dir' with force of 'boost'
        FURI_LOG_I(TAG, "TURBO! dir: %.3f,%.3f", (double)dir.x, (double)dir.y);
        ball.accelerate(dir * (boost / 50.0f));
    }
    return false;
}

Plunger::Plunger(const Vec2& p_)
    : Object(p_, 20)
    , size(100) {
    compression = 0;
}

void Plunger::draw(Canvas* canvas) {
    // draw the end / striker
    canvas_draw_circle(canvas, p.x / 10, p.y / 10, r / 10);
    // draw a line, adjusted for compression
    // canvas_draw_line(
    //     canvas,
    //     roundf(p.x),
    //     roundf(p.y),
    //     roundf(p2.x),
    //     //roundf(me->p.y - (plunger->size - plunger->compression))
    //     roundf(p2.y));
}

void Chaser::draw(Canvas* canvas) {
    Vec2& p1 = points[0];
    Vec2& p2 = points[1];

    // TODO: feels like we can do all this with less code?
    switch(style) {
    case Style::SLASH: // / / / / / / / / /
        if(p1.x == p2.x) {
            int start = p1.y;
            int end = p2.y;
            if(start < end) {
                for(int y = start + offset; y < end; y += gap) {
                    canvas_draw_line(canvas, p1.x - 2, y + 2, p1.x + 2, y - 2);
                }
            } else {
                for(int y = start - offset; y > end; y -= gap) {
                    canvas_draw_line(canvas, p1.x - 2, y + 2, p1.x + 2, y - 2);
                }
            }
        } else if(p1.y == p2.y) {
            int start = p1.x;
            int end = p2.x;
            if(start < end) {
                for(int x = start + offset; x < end; x += gap) {
                    canvas_draw_line(canvas, x - 2, p1.y + 2, x + 2, p1.y - 2);
                }
            } else {
                for(int x = start - offset; x > end; x -= gap) {
                    canvas_draw_line(canvas, x - 2, p1.y + 2, x + 2, p1.y - 2);
                }
            }
        }
        break;
    default: // Style::SIMPLE, just dots
        // for all pixels between p and q, draw them with offset and gap
        if(p1.x == p2.x) {
            int start = p1.y;
            int end = p2.y;
            if(start < end) {
                for(int y = start + offset; y < end; y += gap) {
                    canvas_draw_disc(canvas, p1.x, y, 1);
                }
            } else {
                for(int y = start - offset; y > end; y -= gap) {
                    canvas_draw_disc(canvas, p1.x, y, 1);
                }
            }
        } else if(p1.y == p2.y) {
            int start = p1.x;
            int end = p2.x;
            if(start < end) {
                for(int x = start + offset; x < end; x += gap) {
                    canvas_draw_disc(canvas, x, p1.y, 1);
                }
            } else {
                for(int x = start - offset; x > end; x -= gap) {
                    canvas_draw_disc(canvas, x, p1.y, 1);
                }
            }
        }
        break;
    }
}

void Chaser::step_animation() {
    tick++;
    if(tick % (speed) == 0) {
        offset = (offset + 1) % gap;
    }
}
