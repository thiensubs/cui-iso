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

/* Two solids at the same depth key, drawn and picked: the sort paints the
 * later insertion on top, so picking must name it too. The pixel is found by
 * scanning, so the test does not carry a coordinate that can drift. */
static int tie_break(cui_iso_scene *sc) {
    float sx = -1.0f, sy = -1.0f;
    cui_iso_begin(sc, &CAM);
    cui_iso_sphere(sc, -30.0f, 20.0f, 40.0f, 26.0f, CUI_RGB(0xffffff));
    cui_iso_sphere(sc, 30.0f, -20.0f, 40.0f, 26.0f, CUI_RGB(0xffffff));
    cui_iso_end(sc, NULL);
    for (int y = 0; y < H && sy < 0; y++)
        for (int x = 0; x < W; x++)
            if (cui_iso_pick(sc, (float)x, (float)y) >= 0) { sx = (float)x; sy = (float)y; goto found; }
found:
    if (sy < 0) {
        fprintf(stderr, "FAIL tie-break: no pixel selected either sphere\n");
        return 0;
    }
    int front = cui_iso_pick(sc, sx, sy);
    /* swap the insertion order and ask again; the answer must follow the paint */
    cui_iso_begin(sc, &CAM);
    cui_iso_sphere(sc, 30.0f, -20.0f, 40.0f, 26.0f, CUI_RGB(0xffffff));
    cui_iso_sphere(sc, -30.0f, 20.0f, 40.0f, 26.0f, CUI_RGB(0xffffff));
    cui_iso_end(sc, NULL);
    int other = cui_iso_pick(sc, sx, sy);
    if (front == other) {
        fprintf(stderr, "FAIL tie-break: pick named %d both ways; the later "
                        "insertion must win\n", front);
        return 0;
    }
    printf("ok   tie-break: pick follows the painted order (%d, then %d)\n",
           front, other);
    return 1;
}

/* A click outside the bounds the scene clips to must select nothing, and one
 * inside must still select a solid that is only partly clipped. The click point
 * is found by scanning. */
static int bounds_respected(cui_iso_scene *sc) {
    cui_iso_begin(sc, &CAM);
    cui_iso_box(sc, 0, 0, 0, 80, 80, 60, 12,
                CUI_RGB(0xffffff), CUI_RGB(0xffffff));
    cui_iso_end(sc, NULL);
    float sx = -1.0f, sy = -1.0f;
    for (int y = 0; y < H && sy < 0; y++)
        for (int x = 0; x < W; x++)
            if (cui_iso_pick(sc, (float)x, (float)y) >= 0) { sx = (float)x; sy = (float)y; goto found; }
found:
    if (sy < 0) {
        fprintf(stderr, "FAIL bounds: the box selected nothing at all\n");
        return 0;
    }
    /* bounds that exclude the found point */
    cui_rect out = { 0.0f, 0.0f, sx - 8.0f, (float)H };
    cui_iso_set_bounds(sc, out);
    if (cui_iso_pick(sc, sx, sy) >= 0) {
        fprintf(stderr, "FAIL bounds: a click the bounds exclude selected a "
                        "solid\n");
        return 0;
    }
    /* bounds that exclude nothing: clipped or not, that point still hits */
    cui_rect in = { 0.0f, 0.0f, (float)W, (float)H };
    cui_iso_set_bounds(sc, in);
    if (cui_iso_pick(sc, sx, sy) < 0) {
        fprintf(stderr, "FAIL bounds: a click inside the bounds selected "
                        "nothing\n");
        return 0;
    }
    cui_iso_set_bounds(sc, (cui_rect){ 0.0f, 0.0f, -1.0f, -1.0f });
    printf("ok   bounds: excluded click rejected, in-bounds click still picks\n");
    return 1;
}

/* The clickable area of a box is its chamfered silhouette, not the rectangle
 * it was built from. A click in the corner the chamfer cut away must select
 * nothing, and a click just inside the cut must select the box.
 *
 * World (-38, -38) is inside the 80x80 footprint but inside the corner a
 * 12-unit chamfer removes; world (-27, -39) is just past the cut edge. */
static int chamfered_box_pick(void) {
    cui_iso_scene *sc = cui_iso_scene_create();
    cui_iso_begin(sc, &CAM);
    cui_iso_box(sc, 0, 0, 0, 80, 80, 60, 12,
                CUI_RGB(0xffffff), CUI_RGB(0xffffff));
    cui_iso_end(sc, NULL);

    cui_vec2 out = cui_iso_project(&CAM, -38.0f, -38.0f, 0.0f);
    cui_vec2 in = cui_iso_project(&CAM, -27.0f, -39.0f, 0.0f);
    int hit_out = cui_iso_pick(sc, out.x, out.y);
    int hit_in = cui_iso_pick(sc, in.x, in.y);
    cui_iso_scene_destroy(sc);

    if (hit_out >= 0) {
        fprintf(stderr, "FAIL chamfer: a click in the cut-away corner at "
                        "(%.1f, %.1f) selected solid %d\n",
                out.x, out.y, hit_out);
        return 0;
    }
    if (hit_in < 0) {
        fprintf(stderr, "FAIL chamfer: a click inside the shape at "
                        "(%.1f, %.1f) selected nothing\n", in.x, in.y);
        return 0;
    }
    printf("ok   chamfer: the corner the chamfer cut away is not pickable\n");
    return 1;
}

/* A context with a software renderer, for the tests that need a real paint:
 * the clip a scene was painted under is only known once a frame was drawn. */
typedef struct { cui_sw_renderer *sw; cui_ctx *ui; } env;

static void env_make(env *e) {
    e->sw = cui_sw_create();
    e->ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(e->sw),
                                         .theme = CUI_THEME_DARK });
    cui_set_viewport(e->ui, W, H, 1.0f);
}

static void env_free(env *e) {
    cui_destroy(e->ui);
    cui_sw_destroy(e->sw);
}

/* Picking answers about the pixels that were painted, so it uses the clip of
 * the last successful paint. The bounds a caller sets after that paint are a
 * request for the next frame: they do not change what is on screen, and they
 * must not change what a click selects either. */
static int painted_clip_is_what_pick_uses(void) {
    env e;
    env_make(&e);
    cui_iso_scene *sc = cui_iso_scene_create();

    /* Paint the box with bounds that cut it in half: only the part left of
     * x = 130 reaches the screen. */
    cui_iso_set_bounds(sc, (cui_rect){ 0, 0, 130, (float)H });
    cui_begin_frame(e.ui, 1.0f / 60.0f);
    cui_iso_begin(sc, &CAM);
    cui_iso_box(sc, 0, 0, 0, 80, 80, 60, 12,
                CUI_RGB(0xffffff), CUI_RGB(0xffffff));
    cui_iso_end(sc, e.ui);
    cui_end_frame(e.ui);

    /* The right half of the box is painted away, so a click there selects
     * nothing; the left half is on the screen, so a click there selects the
     * box. */
    cui_vec2 right = cui_iso_project(&CAM, 25.0f, 0.0f, 0.0f);
    cui_vec2 left = cui_iso_project(&CAM, -25.0f, 0.0f, 0.0f);
    int ok = 1;
    if (cui_iso_pick(sc, right.x, right.y) >= 0) {
        fprintf(stderr, "FAIL painted clip: a click on the part the bounds "
                        "clipped away selected a solid\n");
        ok = 0;
    }
    if (cui_iso_pick(sc, left.x, left.y) < 0) {
        fprintf(stderr, "FAIL painted clip: a click on the visible part "
                        "selected nothing\n");
        ok = 0;
    }

    /* Widening the bounds after the paint is a request for the next frame.
     * The pixels have not moved, so the click that hit nothing must still hit
     * nothing. */
    cui_iso_set_bounds(sc, (cui_rect){ 0, 0, (float)W, (float)H });
    if (cui_iso_pick(sc, right.x, right.y) >= 0) {
        fprintf(stderr, "FAIL painted clip: widening the bounds after the "
                        "paint made a clipped-away solid selectable\n");
        ok = 0;
    }

    /* The other way round: paint the box whole, then narrow the bounds. The
     * solid that is on the screen stays selectable. */
    cui_begin_frame(e.ui, 1.0f / 60.0f);
    cui_iso_begin(sc, &CAM);
    cui_iso_box(sc, 0, 0, 0, 80, 80, 60, 12,
                CUI_RGB(0xffffff), CUI_RGB(0xffffff));
    cui_iso_end(sc, e.ui);
    cui_end_frame(e.ui);
    cui_iso_set_bounds(sc, (cui_rect){ 0, 0, 60, (float)H });
    if (cui_iso_pick(sc, right.x, right.y) < 0) {
        fprintf(stderr, "FAIL painted clip: narrowing the bounds after the "
                        "paint took away a solid that is on the screen\n");
        ok = 0;
    }

    cui_iso_scene_destroy(sc);
    env_free(&e);
    if (ok) printf("ok   painted clip: picking follows the paint, not the "
                   "bounds set afterwards\n");
    return ok;
}

/* The host's own clip decides what reaches the screen, so picking has to
 * agree with it: a click on a solid the host clipped away selects nothing,
 * and a click on the part that is visible still selects it. */
static int host_clip_is_respected(void) {
    env e;
    env_make(&e);
    cui_iso_scene *sc = cui_iso_scene_create();

    /* the host clips the paint to the left half of the viewport */
    cui_begin_frame(e.ui, 1.0f / 60.0f);
    cui_push_clip(e.ui, (cui_rect){ 0, 0, 130, (float)H });
    cui_iso_begin(sc, &CAM);
    cui_iso_box(sc, 0, 0, 0, 80, 80, 60, 12,
                CUI_RGB(0xffffff), CUI_RGB(0xffffff));
    cui_iso_end(sc, e.ui);
    cui_pop_clip(e.ui);
    cui_end_frame(e.ui);

    cui_vec2 right = cui_iso_project(&CAM, 25.0f, 0.0f, 0.0f);
    cui_vec2 left = cui_iso_project(&CAM, -25.0f, 0.0f, 0.0f);
    int ok = 1;
    if (cui_iso_pick(sc, right.x, right.y) >= 0) {
        fprintf(stderr, "FAIL host clip: a click on the part the host "
                        "clipped away selected a solid\n");
        ok = 0;
    }
    if (cui_iso_pick(sc, left.x, left.y) < 0) {
        fprintf(stderr, "FAIL host clip: a click on the visible part "
                        "selected nothing\n");
        ok = 0;
    }

    cui_iso_scene_destroy(sc);
    env_free(&e);
    if (ok) printf("ok   host clip: picking agrees with the host's clip\n");
    return ok;
}

int main(void) {
    long p, d, f;
    int ok = 1;
    ok &= check("box", 0, &p, &d, &f);
    ok &= check("cylinder", 1, &p, &d, &f);

    cui_iso_scene *sc = cui_iso_scene_create();
    ok &= tie_break(sc);
    ok &= bounds_respected(sc);
    cui_iso_scene_destroy(sc);
    ok &= chamfered_box_pick();
    ok &= painted_clip_is_what_pick_uses();
    ok &= host_clip_is_respected();
    return ok ? 0 : 1;
}
