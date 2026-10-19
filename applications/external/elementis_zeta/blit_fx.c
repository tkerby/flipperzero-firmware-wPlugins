// blit_masked_fx (battle picture effects): lives in the battle plugin (battle_plugin.c) on the device.
#include "blit.h"
#ifndef NUCLEO_DEVICE
unsigned
    g_blit_fx_calls; // host tests only: how many pictures went through the per-pixel effect blit (contract 52: none once the legendary intro is over)
#endif

void blit_masked_fx(LvlBuf lvl, const Blit* v, int x, int y, const BlitFx* fx) {
#ifndef NUCLEO_DEVICE
    g_blit_fx_calls++;
#endif
    int ow = v->w, oh = v->h, x0 = x, y0 = y;
    const bool scaled = fx->sc_d && fx->sc_n != fx->sc_d;
    if(scaled) {
        ow = (v->w * fx->sc_n) / fx->sc_d;
        oh = (v->h * fx->sc_n) / fx->sc_d;
        if(ow < 1) ow = 1;
        if(oh < 1) oh = 1;
        x0 = x + v->w / 2 - ow / 2;
        y0 = y + v->h - oh;
    }
    for(int oy = 0; oy < oh; oy++) {
        int py = y0 + oy;
        if(py < 0 || py >= SCREEN_H) continue;
        if(fx->clip_y && py >= fx->clip_y) break;
        int sy = scaled ? (oy * v->h) / oh : oy;
        const uint8_t* mrow = v->mask + sy * v->mbpr;
        for(int ox = 0; ox < ow; ox++) {
            int px = x0 + ox;
            if(px < 0 || px >= SCREEN_W) continue;
            int sx = scaled ? (ox * v->w) / ow : ox;
            if(v->flip) sx = v->w - 1 - sx;
            if(!((mrow[sx >> 3] >> (sx & 7)) & 1)) continue;
            uint8_t l = blit_px(v, sy, sx);
            if(fx->flat && py >= fx->real_y)
                l = (uint8_t)(fx->flat - 1);
            else if(fx->invert)
                l = (uint8_t)(3 - l);
            lvl_put(lvl, px, py, l);
        }
    }
}
