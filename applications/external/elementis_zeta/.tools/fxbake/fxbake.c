// fxbake: renders the battle effects (the reference painters in fxref.c, which are the ARTWORK) into nucleo/files/fx.bin
// (format: nucleo/fx_tab.h, contract 10.4).  Host tool, no Flipper SDK.
//   fxbake OUT.bin            bake everything
//   fxbake OUT.bin -v         + per kind sizes
//
// Every frame is rendered with the painters on a canvas bigger than the screen (so nothing is clipped; the player clips
// like the painters did), cropped to the painted pixels and stored as run-length rows.  The box the painters work on is
// the class box (W x FXC_H, or 18 x 20 for the crystal); pieces are stored relative to its middle column / bottom row.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fx_tab.h"

#define CW 400
#define CH 300
// The painters work in SCREEN coordinates and their rounding depends on them a little (an int cast of a negative float truncates
// toward zero), so a box is baked where the game puts it: the enemy's picture, right edge at x = 126, top row 2 (a box of the
// player's creature is 1 px further down and at the left edge; the difference is a particle moving by a pixel).  The canvas is the
// screen plus a margin: canvas = painter coordinates + (CAN_X, CAN_Y).
#define CAN_X 100
#define CAN_Y 100
#define BOX_RIGHT 126
#define BOX_TOP 2

static uint8_t cv[CH][CW]; // bit 2: painted, bits 0..1: level (0 white, 3 black)
static int bx0, by0, bx1, by1;
int bk_flash;
static int g_oob;

static void touch(int x, int y) {
    if(x < bx0) bx0 = x;
    if(x > bx1) bx1 = x;
    if(y < by0) by0 = y;
    if(y > by1) by1 = y;
}
void bk_put(int x, int y, uint8_t v) {
    x += CAN_X;
    y += CAN_Y;
    if(x < 0 || y < 0 || x >= CW || y >= CH) {
        g_oob++;
        return;
    }
    cv[y][x] = (uint8_t)(4 | (v & 3));
    touch(x, y);
}
void bk_span(int y, int x0, int x1, uint8_t v) {
    for(int x = x0; x < x1; x++) bk_put(x, y, v);
}
static void clear(void) {
    if(bx1 >= 0)
        for(int y = by0; y <= by1; y++) memset(&cv[y][bx0], 0, (size_t)(bx1 - bx0 + 1));
    bx0 = by0 = 1 << 20;
    bx1 = by1 = -1;
    bk_flash = 0;
}

void ref_fx_draw(void* lvl, uint8_t kind, uint16_t t, int x0, int y0, int x1, int y1, int dir, int big);
void ref_proj_part(uint8_t kind, uint16_t t, int part, int right, int hx, int hy);

// ------------------------------------------------------------------------------------------------ byte buffers
typedef struct {
    uint8_t* d;
    int n, cap;
} Buf;
static void bput(Buf* b, int v) {
    if(b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 1024;
        b->d = realloc(b->d, (size_t)b->cap);
    }
    b->d[b->n++] = (uint8_t)v;
}

// The painted canvas as one piece (rows of runs) relative to (rx, ry). Returns false when nothing is painted.
static bool enc_piece(Buf* b, int rx, int ry, int tr) {
    if(bx1 < 0) return false;
    int w = bx1 - bx0 + 1, h = by1 - by0 + 1;
    int dx = bx0 - CAN_X - rx, dy = by0 - CAN_Y - ry;
    if(dx < -128 || dx > 127 || dy < -128 || dy > 127 || w > 255 || h > 255) {
        fprintf(stderr, "fxbake: piece out of range dx %d dy %d w %d h %d\n", dx, dy, w, h);
        exit(1);
    }
    bput(b, dx);
    bput(b, dy);
    bput(b, w);
    bput(b, h);
    bput(b, tr);
    for(int y = by0; y <= by1; y++) {
        int x = bx0;
        while(x <= bx1) {
            uint8_t c = cv[y][x];
            int code = (c & 4) ? ((c & 3) ? 3 : 1) : 0;
            int n = 1;
            while(x + n <= bx1 && n < 64) {
                uint8_t c2 = cv[y][x + n];
                int code2 = (c2 & 4) ? ((c2 & 3) ? 3 : 1) : 0;
                if(code2 != code) break;
                n++;
            }
            bput(b, code << 6 | (n - 1));
            x += n;
        }
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ animations
typedef struct {
    Buf fr;            // frame data
    uint16_t t[FXF_MAXKEYS + 1], off[FXF_MAXKEYS + 1];
    int nk, maxf;
} Anim;

static void anim_add(Anim* a, int t, const Buf* frame) {
    int last_off = a->nk ? a->off[a->nk - 1] : 0;
    if(a->nk && frame->n == a->fr.n - last_off && !memcmp(frame->d, a->fr.d + last_off, (size_t)frame->n)) return; // same as the last key
    if(a->nk >= FXF_MAXKEYS) {
        fprintf(stderr, "fxbake: too many keys\n");
        exit(1);
    }
    a->t[a->nk] = (uint16_t)t;
    a->off[a->nk] = (uint16_t)a->fr.n;
    if(a->fr.n + frame->n > 65535) {
        fprintf(stderr, "fxbake: animation > 64 KB\n");
        exit(1);
    }
    for(int i = 0; i < frame->n; i++) bput(&a->fr, frame->d[i]);
    if(frame->n > a->maxf) a->maxf = frame->n;
    a->nk++;
}
static void anim_blob(const Anim* a, Buf* out) {
    for(int i = 0; i < a->nk; i++) {
        bput(out, a->t[i] & 255);
        bput(out, a->t[i] >> 8);
        bput(out, a->off[i] & 255);
        bput(out, a->off[i] >> 8);
    }
    for(int i = 0; i < a->fr.n; i++) bput(out, a->fr.d[i]);
}

// the effect over a W x H box at its time t (the box: see BOX_RIGHT)
static void frame_fx(Buf* f, int kind, int big, int dir, int W, int H, int t) {
    int x0 = H == FXC_SMALL_H ? 60 : BOX_RIGHT - W, y0 = H == FXC_SMALL_H ? 20 : BOX_TOP;
    clear();
    ref_fx_draw(NULL, (uint8_t)kind, (uint16_t)t, x0, y0, x0 + W, y0 + H, dir, big);
    int np = bx1 >= 0 ? 1 : 0;
    bput(f, (np << 1) | (bk_flash ? 1 : 0));
    if(np) enc_piece(f, (x0 + x0 + W) / 2, y0 + H, 0);
}
static void bake_fx(Anim* a, int kind, int big, int dir, int W, int H) {
    memset(a, 0, sizeof *a);
    for(int t = 0; t < (int)fxt_duration((uint8_t)kind); t += FX_KS[kind]) {
        Buf f = {0};
        frame_fx(&f, kind, big, dir, W, H, fxt_sample((uint8_t)kind, (unsigned)t, FX_KS[kind]));
        anim_add(a, t, &f);
        free(f.d);
    }
}
// a projectile's sprites (head at (60, 30)): trail discs 3 2 1, then the head; the swarm needle has the head only
static void bake_proj(Anim* a, int kind, int right) {
    memset(a, 0, sizeof *a);
    for(int t = 0; t < FXP_MS; t += FXP_STEP) {
        Buf f = {0};
        int np = 0;
        Buf body = {0};
        for(int part = kind == FX_SWARM ? 0 : 3; part >= 0; part--) {
            clear();
            ref_proj_part((uint8_t)kind, (uint16_t)(t + FXP_STEP / 2), part, right, 60, 30);
            if(enc_piece(&body, 60, 30, part)) np++;
        }
        bput(&f, np << 1);
        for(int i = 0; i < body.n; i++) bput(&f, body.d[i]);
        anim_add(a, t, &f);
        free(f.d);
        free(body.d);
    }
}

// ------------------------------------------------------------------------------------------------ the file
typedef struct {
    Buf blob;
    int nk, maxf, total;
    uint32_t off;
} Blob;
static Blob blobs[4096];
static int nblobs;
static int store(const Anim* a) {
    Buf b = {0};
    anim_blob(a, &b);
    for(int i = 0; i < nblobs; i++)
        if(blobs[i].blob.n == b.n && !memcmp(blobs[i].blob.d, b.d, (size_t)b.n)) {
            free(b.d);
            return i;
        }
    blobs[nblobs] = (Blob){b, a->nk, a->maxf, a->fr.n, 0};
    return nblobs++;
}


static int ent_blob[FXF_NENT];

int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(stderr, "usage: fxbake OUT.bin [-v]\n");
        return 1;
    }
    bool verbose = argc > 2 && !strcmp(argv[2], "-v");
    for(int i = 0; i < FXF_NENT; i++) ent_blob[i] = -1;
    bx1 = -1;
    clear();
    for(int kind = 1; kind < FX_COUNT; kind++) {
        for(int big = 0; big < 2; big++)
            for(int dir = -1; dir <= 1; dir += 2) {
                int baked[FXC_N];
                for(int c = 0; c < FXC_N; c++) baked[c] = -1;
                for(int c = 1; c < FXC_N; c++) {
                    if(!(FX_WMASK[kind] & (1 << (c - 1)))) continue;
                    Anim a;
                    bake_fx(&a, kind, big, dir, FXC_W[c], FXC_H);
                    baked[c] = store(&a);
                    free(a.fr.d);
                }
                if(kind == FX_STAR || kind == FX_STARS3) { // the capture crystal's box
                    Anim a;
                    bake_fx(&a, kind, 0, 1, FXC_W[0], FXC_SMALL_H);
                    baked[0] = store(&a);
                    free(a.fr.d);
                }
                for(int c = 1; c < FXC_N; c++) // alias the unbaked classes
                    if(baked[c] < 0) {
                        int best = -1;
                        for(int d = 1; d < FXC_N && best < 0; d++) {
                            if(c + d < FXC_N && baked[c + d] >= 0) best = baked[c + d];
                            else if(c - d >= 1 && baked[c - d] >= 0) best = baked[c - d];
                        }
                        baked[c] = best;
                    }
                if(baked[0] < 0) baked[0] = baked[1];
                for(int c = 0; c < FXC_N; c++) ent_blob[fxt_index((unsigned)kind, (unsigned)big, dir, (unsigned)c)] = baked[c];
            }
        for(int right = 0; right < 2; right++) {
            if(kind == FX_BOLT) continue; // the beam is drawn by the player (it joins the user to the head)
            Anim a;
            bake_proj(&a, kind, right);
            ent_blob[fxt_pindex((unsigned)kind, right)] = store(&a);
            free(a.fr.d);
        }
    }
    // layout
    uint32_t off = FXF_HDR + FXF_ENT * FXF_NENT;
    for(int i = 0; i < nblobs; i++) {
        blobs[i].off = off;
        off += (uint32_t)blobs[i].blob.n;
    }
    FILE* f = fopen(argv[1], "wb");
    if(!f) {
        perror(argv[1]);
        return 1;
    }
    uint8_t hdr[FXF_HDR] = {'N', 'F', 'X', '1', 1, FX_COUNT, FXC_N, FXF_ENT, FXF_NENT & 255, FXF_NENT >> 8, 0, 0, FXC_W[0], FXC_W[1], FXC_W[2], FXC_W[3]};
    fwrite(hdr, 1, sizeof hdr, f);
    for(int i = 0; i < FXF_NENT; i++) {
        uint8_t e[FXF_ENT] = {0};
        int bi = ent_blob[i];
        if(bi >= 0) {
            const Blob* b = &blobs[bi];
            e[0] = b->off & 255;
            e[1] = (b->off >> 8) & 255;
            e[2] = (b->off >> 16) & 255;
            e[3] = (b->off >> 24) & 255;
            e[4] = b->total & 255;
            e[5] = b->total >> 8;
            e[6] = b->maxf & 255;
            e[7] = b->maxf >> 8;
            e[8] = (uint8_t)b->nk;
        }
        fwrite(e, 1, sizeof e, f);
    }
    int total_blob = 0, maxtotal = 0, maxfr = 0, maxnk = 0;
    for(int i = 0; i < nblobs; i++) {
        fwrite(blobs[i].blob.d, 1, (size_t)blobs[i].blob.n, f);
        total_blob += blobs[i].blob.n;
        if(blobs[i].total > maxtotal) maxtotal = blobs[i].total;
        if(blobs[i].maxf > maxfr) maxfr = blobs[i].maxf;
        if(blobs[i].nk > maxnk) maxnk = blobs[i].nk;
    }
    fclose(f);
    printf("fxbake: %s  %d animations, %u bytes (directory %d, animations %d); largest animation %d B, largest frame %d B, most keys %d; canvas overflow %d\n",
           argv[1], nblobs, (unsigned)off, FXF_ENT * FXF_NENT, total_blob, maxtotal, maxfr, maxnk, g_oob);
    if(verbose) {
        for(int k = 1; k < FX_COUNT; k++) {
            unsigned idx = fxt_index((unsigned)k, 0, 1, 2);
            int bi = ent_blob[idx];
            printf("  kind %2d: %2d keys, %6d B (44 class), max frame %4d\n", k, blobs[bi].nk, blobs[bi].total, blobs[bi].maxf);
        }
    }
    return g_oob ? 2 : 0;
}
