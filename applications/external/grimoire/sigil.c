#include "sigil.h"
#include <math.h>
#include <string.h>
#include <ctype.h>

// The firmware builds with single-precision constants; these must stay double to match the site's doubles.
#define SIGIL_PI 3.14159265358979323846L

// ---- the letters, as strokes in a box from -1 to 1, y down -----------------
// A stroke is either a polyline or an arc (centre, radius, from, to, in degrees).

typedef struct {
    bool arc;
    uint8_t n; // polyline points
    const double* pts; // x0, y0, x1, y1, ...
    double cx, cy, r, a0, a1; // arc
} Stroke;

#define P(name, ...)           static const double name[] = {__VA_ARGS__};
#define POLY(name)             {false, sizeof(name) / sizeof(double) / 2, name, 0, 0, 0, 0, 0}
#define ARC(cx, cy, r, a0, a1) {true, 0, 0, cx, cy, r, a0, a1}

P(A1, -1, 1, 0, -1, 1, 1)
P(A2, -0.5f, 0.2f, 0.5f, 0.2f)
P(B1, -0.7f, -1, -0.7f, 1)
P(D1, -0.7f, -1, -0.7f, 1)
P(E1, 0.8f, -1, -0.8f, -1, -0.8f, 1, 0.8f, 1)
P(E2, -0.8f, 0, 0.5f, 0)
P(F1, 0.8f, -1, -0.8f, -1, -0.8f, 1) P(F2, -0.8f, 0, 0.5f, 0) P(G2, 1, 0, 0.1f, 0)
    P(H1, -0.8f, -1, -0.8f, 1) P(H2, 0.8f, -1, 0.8f, 1) P(H3, -0.8f, 0, 0.8f, 0) P(I1, 0, -1, 0, 1)
        P(J1, 0.5f, -1, 0.5f, 0.4f) P(K1, -0.8f, -1, -0.8f, 1) P(K2, 0.8f, -1, -0.8f, 0.1f)
            P(K3, -0.3f, -0.3f, 0.8f, 1) P(L1, -0.8f, -1, -0.8f, 1, 0.8f, 1)
                P(M1, -1, 1, -1, -1, 0, 0.2f, 1, -1, 1, 1)
                    P(N1, -0.8f, 1, -0.8f, -1, 0.8f, 1, 0.8f, -1) P(P1, -0.7f, 1, -0.7f, -1)
                        P(Q2, 0.4f, 0.4f, 1.1f, 1.1f) P(R1, -0.7f, 1, -0.7f, -1)
                            P(R3, -0.4f, 0, 0.8f, 1) P(T1, -0.9f, -1, 0.9f, -1) P(T2, 0, -1, 0, 1)
                                P(U1, -0.8f, -1, -0.8f, 0.3f) P(U3, 0.8f, 0.3f, 0.8f, -1)
                                    P(V1, -1, -1, 0, 1, 1, -1)
                                        P(W1, -1, -1, -0.5f, 1, 0, -0.2f, 0.5f, 1, 1, -1)
                                            P(X1, -1, -1, 1, 1) P(X2, 1, -1, -1, 1)
                                                P(Y1, -1, -1, 0, 0, 1, -1) P(Y2, 0, 0, 0, 1)
                                                    P(Z1, -0.9f, -1, 0.9f, -1, -0.9f, 1, 0.9f, 1)

                                                        static const Stroke glyph_A[] = {
                                                            POLY(A1),
                                                            POLY(A2)};
static const Stroke glyph_B[] = {
    POLY(B1),
    ARC(-0.7f, -0.5f, 0.5f, -90, 90),
    ARC(-0.7f, 0.5f, 0.5f, -90, 90)};
static const Stroke glyph_C[] = {ARC(0, 0, 1, 40, 320)};
static const Stroke glyph_D[] = {POLY(D1), ARC(-0.7f, 0, 1, -90, 90)};
static const Stroke glyph_E[] = {POLY(E1), POLY(E2)};
static const Stroke glyph_F[] = {POLY(F1), POLY(F2)};
static const Stroke glyph_G[] = {ARC(0, 0, 1, 40, 360), POLY(G2)};
static const Stroke glyph_H[] = {POLY(H1), POLY(H2), POLY(H3)};
static const Stroke glyph_I[] = {POLY(I1)};
static const Stroke glyph_J[] = {POLY(J1), ARC(0, 0.4f, 0.5f, 0, 180)};
static const Stroke glyph_K[] = {POLY(K1), POLY(K2), POLY(K3)};
static const Stroke glyph_L[] = {POLY(L1)};
static const Stroke glyph_M[] = {POLY(M1)};
static const Stroke glyph_N[] = {POLY(N1)};
static const Stroke glyph_O[] = {ARC(0, 0, 1, 0, 360)};
static const Stroke glyph_P[] = {POLY(P1), ARC(-0.7f, -0.5f, 0.5f, -90, 90)};
static const Stroke glyph_Q[] = {ARC(0, 0, 1, 0, 360), POLY(Q2)};
static const Stroke glyph_R[] = {POLY(R1), ARC(-0.7f, -0.5f, 0.5f, -90, 90), POLY(R3)};
static const Stroke glyph_S[] = {ARC(0, -0.5f, 0.5f, -30, -270), ARC(0, 0.5f, 0.5f, -90, 150)};
static const Stroke glyph_T[] = {POLY(T1), POLY(T2)};
static const Stroke glyph_U[] = {POLY(U1), ARC(0, 0.3f, 0.8f, 180, 0), POLY(U3)};
static const Stroke glyph_V[] = {POLY(V1)};
static const Stroke glyph_W[] = {POLY(W1)};
static const Stroke glyph_X[] = {POLY(X1), POLY(X2)};
static const Stroke glyph_Y[] = {POLY(Y1), POLY(Y2)};
static const Stroke glyph_Z[] = {POLY(Z1)};

typedef struct {
    const Stroke* strokes;
    uint8_t count;
} Glyph;
#define G(l) {glyph_##l, sizeof(glyph_##l) / sizeof(Stroke)}
static const Glyph glyphs[26] = {G(A), G(B), G(C), G(D), G(E), G(F), G(G), G(H), G(I),
                                 G(J), G(K), G(L), G(M), G(N), G(O), G(P), G(Q), G(R),
                                 G(S), G(T), G(U), G(V), G(W), G(X), G(Y), G(Z)};

// ---- the wish, reduced; the sigil, composed --------------------------------

static bool vowel(char c) {
    return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
}

size_t sigil_reduce(const char* wish, char* out, size_t cap) {
    char all[27];
    size_t n = 0;
    bool seen[26] = {0};
    for(const char* p = wish; *p && n < 26; p++) {
        char c = toupper((unsigned char)*p);
        if(c < 'A' || c > 'Z' || seen[c - 'A']) continue;
        seen[c - 'A'] = true;
        all[n++] = c;
    }
    size_t k = 0;
    for(size_t i = 0; i < n && k + 1 < cap; i++) {
        if(!vowel(all[i])) out[k++] = all[i];
    }
    if(k == 0) {
        for(size_t i = 0; i < n && k + 1 < cap; i++)
            out[k++] = all[i]; // nothing but vowels keeps them
    }
    out[k] = 0;
    return k;
}

// FNV-1a over the wish's letters, as the site hashes them
static uint32_t hash_letters(const char* wish) {
    uint32_t h = 2166136261u;
    for(const char* p = wish; *p; p++) {
        char c = toupper((unsigned char)*p);
        if(c < 'A' || c > 'Z') continue;
        h ^= (uint8_t)c;
        h *= 16777619u;
    }
    return h;
}

// mulberry32, bit for bit
typedef struct {
    uint32_t a;
} Rng;

static double rng_next(Rng* r) {
    r->a += 0x6D2B79F5u;
    uint32_t t = r->a;
    t = (t ^ (t >> 15)) * (1u | t);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return (double)((t ^ (t >> 14)) >> 0) / 4294967296.0L;
}

static bool push_point(Sigil* s, float x, float y) {
    if(s->point_count >= SIGIL_MAX_POINTS) return false;
    s->points[s->point_count].x = x;
    s->points[s->point_count].y = y;
    s->point_count++;
    return true;
}

static bool begin_stroke(Sigil* s) {
    if(s->stroke_count >= SIGIL_MAX_STROKES) return false;
    s->strokes[s->stroke_count].first = s->point_count;
    s->strokes[s->stroke_count].count = 0;
    return true;
}

static void end_stroke(Sigil* s) {
    s->strokes[s->stroke_count].count = s->point_count - s->strokes[s->stroke_count].first;
    if(s->strokes[s->stroke_count].count) s->stroke_count++;
}

// Arc points as arcPts() makes them: at least six segments, one per 15 degrees
static int arc_steps(double a0, double a1) {
    int n = (int)lround(fabs(a1 - a0) / 15.0L);
    return n < 6 ? 6 : n;
}

typedef struct {
    double cs, sn, s, dx, dy;
    int flip;
} Placement;

static void place(Sigil* sig, const Placement* pl, double x, double y) {
    double px = x * pl->flip * pl->s, py = y * pl->s;
    push_point(
        sig,
        (float)(pl->cs * px - pl->sn * py + pl->dx),
        (float)(pl->sn * px + pl->cs * py + pl->dy));
}

static void add_stroke(Sigil* sig, const Stroke* st, const Placement* pl) {
    if(!begin_stroke(sig)) return;
    if(st->arc) {
        int n = arc_steps(st->a0, st->a1);
        for(int i = 0; i <= n; i++) {
            double a = (st->a0 + (st->a1 - st->a0) * i / n) * SIGIL_PI / 180.0L;
            place(sig, pl, st->cx + cos(a) * st->r, st->cy + sin(a) * st->r);
        }
    } else {
        for(int i = 0; i < st->n; i++)
            place(sig, pl, st->pts[i * 2], st->pts[i * 2 + 1]);
    }
    end_stroke(sig);
}

bool sigil_make(const char* wish, Sigil* sig) {
    memset(sig, 0, sizeof(*sig));
    size_t n = sigil_reduce(wish, sig->letters, sizeof(sig->letters));
    if(n == 0) return false;
    Rng r = {hash_letters(wish)};
    // Each letter turned to one of the eight directions (a little off), scaled,
    // flipped sometimes, and set a little off the centre, so the strokes cross
    // and share lines; then the binding: a ring round a long wish, a bar under a short one.
    for(size_t i = 0; i < n; i++) {
        const Glyph* g = &glyphs[sig->letters[i] - 'A'];
        double rot = floor(rng_next(&r) * 8) * SIGIL_PI / 4 + (rng_next(&r) - 0.5L) * 0.25L;
        double s = 0.5L + rng_next(&r) * 0.4L;
        double dx = (rng_next(&r) - 0.5L) * 0.5L;
        double dy = (rng_next(&r) - 0.5L) * 0.5L;
        int flip = rng_next(&r) < 0.4L ? -1 : 1;
        Placement pl = {cos(rot), sin(rot), s, dx, dy, flip};
        for(uint8_t k = 0; k < g->count; k++)
            add_stroke(sig, &g->strokes[k], &pl);
    }
    Placement id = {1, 0, 1, 0, 0, 1};
    if(n >= 4) {
        Stroke ring = ARC(0, 0, 1.18f, 0, 360);
        add_stroke(sig, &ring, &id);
    } else {
        static const double bar[] = {-1.1, 1.0, 1.1, 1.0};
        Stroke b = POLY(bar);
        add_stroke(sig, &b, &id);
    }
    return true;
}
