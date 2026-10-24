#include "ghost_icon.h"

/* Small decorative ghost: round dome, round eyes, zigzag bottom, with two
 * real bugs fixed:
 *  - Straight walls: the dome disc (radius r) and the body box now share
 *    the exact same width (2r+1, matching the disc's true diameter at its
 *    widest row) instead of an off-by-one mismatch that kinked the walls.
 *  - Symmetric zigzag: notch boundaries are computed cumulatively
 *    (left_edge + i*width/count) instead of a single division up front, so
 *    the 3 cuts always tile the FULL width exactly - no leftover
 *    uncut strip on one side, regardless of whether w divides evenly.
 * (cx, top_y) is the top-center of the bounding box. */
void ghost_icon_draw(Canvas* canvas, int cx, int top_y, int w, int h) {
    int r = w / 2;
    int dome_mid = top_y + r; /* disc's widest row; straight walls continue below it */
    int ew = 2 * r + 1; /* effective width, matches the disc's true diameter */
    int bottom_y = top_y + h;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_disc(canvas, cx, dome_mid, r);
    canvas_draw_box(canvas, cx - r, dome_mid, ew, bottom_y - dome_mid);

    /* Zigzag feet: cut 3 white notches into the bottom edge, tiling the
     * full width via cumulative boundaries - leftover black between/around
     * them forms the pointed "legs". */
    canvas_set_color(canvas, ColorWhite);
    const int notch_count = 3;
    int left_edge = cx - r;
    int prev_boundary = left_edge;
    for(int i = 1; i <= notch_count; i++) {
        int boundary = left_edge + (ew * i) / notch_count;
        int notch_w = boundary - prev_boundary;
        int notch_cx = (prev_boundary + boundary) / 2;
        canvas_draw_triangle(canvas, notch_cx, bottom_y, (size_t)notch_w, 6, CanvasDirectionBottomToTop);
        prev_boundary = boundary;
    }

    /* Eyes: round. */
    canvas_draw_disc(canvas, cx - r / 2, dome_mid - 1, 2);
    canvas_draw_disc(canvas, cx + r / 2, dome_mid - 1, 2);

    canvas_set_color(canvas, ColorBlack);
}
