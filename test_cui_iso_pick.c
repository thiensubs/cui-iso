/* test_cui_iso_pick.c - a click must land where the pixels are.
 *
 * Renders one solid alone on black, then classifies every pixel:
 *   P = painted, H = reported hit by the matching hit-test.
 * A click on P but not H is a dead click (the shape is drawn but not pickable).
 * A click on H but not P is a phantom click (nothing is there to see).
 * A convex solid under an orthographic projection covers a convex screen
 * region, so both counts must stay small: the tolerance covers antialiasing
 * plus the highlight ring a sphere draws outside its nominal radius.
 * Exit 0 when every case is inside tolerance, 1 otherwise.
 */
#include "cui_iso.h"
#include "cui_iso_internal.h"
#include "cui_iso_opts.h"
#include <cui/cui_render.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 260
#define H 220
/* generous enough for anti-aliased edges and the sphere highlight, tight enough
 * that a wrong hit shape (a box hit box ignoring chamfer, say) still fails. */
#define TOL_FRAC 0.12f

static cui_iso_camera CAM = { 0.55f, W * 0.5f, H * 0.62f, 1.0f };

/* Render exactly one solid on black; return its pixels. */
static unsigned char *paint(int which) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw),
                                             .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, W, H, 1.0f);
    cui_iso_scene *sc = cui_iso_scene_create();

    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_draw_rect(ui, (cui_rect){ 0, 0, W, H }, CUI_CORNERS(0), CUI_RGB(0x000000));
    cui_iso_begin(sc, &CAM);

    if (which == 0)
        cui_iso_box(sc, 0, 0, 0, 80, 80, 60, 12,
                    CUI_RGB(0xffffff), CUI_RGB(0xffffff));
    else
        cui_iso_cylinder(sc, 0, 0, 0, 34, 60, 48,
                         CUI_RGB(0xffffff), CUI_RGB(0xffffff));

    cui_iso_end(sc, ui);
    cui_end_frame(ui);

    const cui_draw_data *dd = cui_get_draw_data(ui);
    size_t bytes = (size_t)dd->width_px * dd->height_px * 4;
    unsigned char *px = malloc(bytes);
    cui_sw_render(sw, dd, px, dd->width_px, dd->height_px, dd->width_px * 4,
                  CUI_SW_RGBA);

    cui_iso_scene_destroy(sc);
    cui_destroy(ui);
    cui_sw_destroy(sw);
    return px;
}

/* Compare one rendered solid against its hit-test. */
static int check(const char *name, int which, long *painted, long *dead,
                 long *phantom) {
    unsigned char *px = paint(which);
    if (!px) return 0;
    long np = 0, nd = 0, nf = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            size_t i = ((size_t)y * W + x) * 4;
            bool is_painted = px[i] || px[i + 1] || px[i + 2];
            bool is_hit;
            if (which == 0)
                is_hit = cui_iso_hit_solid(&CAM, (float)x, (float)y, 0, 0, 0,
                                          60, 80, 80);
            else
                is_hit = cui_iso_hit_cylinder(&CAM, (float)x, (float)y,
                                              0, 0, 0, 34, 60);
            if (is_painted) np++;
            if (is_painted && !is_hit) nd++;
            if (is_hit && !is_painted) nf++;
        }
    }
    free(px);
    *painted = np; *dead = nd; *phantom = nf;

    long worst = nd > nf ? nd : nf;
    long allow = (long)((float)np * TOL_FRAC);
    if (np < 200) {
        fprintf(stderr, "FAIL %s: only %ld painted pixels\n", name, np);
        return 0;
    }
    if (worst > allow) {
        fprintf(stderr, "FAIL %s: painted %ld, dead %ld, phantom %ld "
                        "(worst over allowance %ld)\n", name, np, nd, nf, allow);
        return 0;
    }
    printf("ok   %s: painted %ld, dead %ld, phantom %ld (allow %ld)\n",
           name, np, nd, nf, allow);
    return 1;
}

int main(void) {
    long p, d, f;
    int ok = 1;
    ok &= check("box", 0, &p, &d, &f);
    ok &= check("cylinder", 1, &p, &d, &f);
    return ok ? 0 : 1;
}
