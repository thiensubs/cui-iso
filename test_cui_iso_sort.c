/* test_cui_iso_sort.c - proves cui_iso_end really sorts by depth.
 *
 * Two scenes hold the same solids but are added in opposite order. A correct
 * painter's sort draws them back to front, so both frames come out identical
 * pixel for pixel. Without the sort the later solid paints over the earlier one
 * and the frames differ wherever they overlap.
 *
 * That test cannot tell a correct sort from an inverted one -- an inverted
 * sort is still a sort, so both frames match either way. The depth-order check
 * at the bottom can.
 *
 * Exit 0 = identical and correctly ordered, 1 = differ, wrong order or error.
 */
#include "cui_iso.h"
#include "cui_iso_internal.h"
#include "cui_iso_opts.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 240
#define H 200
#define SOLD 3

static unsigned char *shot(int reversed) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw),
                                             .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, W, H, 1.0f);
    cui_iso_scene *sc = cui_iso_scene_create();
    cui_iso_camera cam = { 0.55f, W * 0.5f, H * 0.55f, 1.0f };

    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_draw_rect(ui, (cui_rect){0, 0, W, H}, CUI_CORNERS(0), CUI_RGB(0x000000));
    cui_iso_begin(sc, &cam);

    for (int pass = 0; pass < SOLD; pass++) {
        int i = reversed ? (SOLD - 1 - pass) : pass;
        if (i == 0)
            cui_iso_box(sc, -55, 0, 0, 10, 90, 70, 6,
                        CUI_RGB(0x3377cc), CUI_RGB(0x224489));
        else if (i == 1)
            cui_iso_box(sc, 0, 0, 0, 10, 90, 70, 6,
                        CUI_RGB(0xcc7733), CUI_RGB(0x883318));
        else
            cui_iso_sphere_named(sc,
                                 .x = 55, .y = 0, .z = 62, .radius = 12,
                                 .color = CUI_RGB(0x33cc77));
    }

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

/* Which of two overlapping boxes is painted on top.
 *
 * The camera convention is in the header: a larger cui_iso_depth is nearer the
 * camera. At this azimuth the box at +x therefore has the larger depth key,
 * and it has to be the one painted on top -- and it has to stay on top when
 * the insertion order is swapped.
 *
 * The sample point is world (0, 0, 30), inside the lid of both boxes: the two
 * lids sit at the same height, so only the depth decides the overlap. */
static int depth_order(int reversed) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw),
                                             .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, W, H, 1.0f);
    cui_iso_scene *sc = cui_iso_scene_create();
    cui_iso_camera cam = { 0.55f, W * 0.5f, H * 0.55f, 1.0f };

    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_draw_rect(ui, (cui_rect){ 0, 0, W, H }, CUI_CORNERS(0), CUI_RGB(0x000000));
    cui_iso_begin(sc, &cam);

    for (int pass = 0; pass < 2; pass++) {
        int i = reversed ? (1 - pass) : pass;
        if (i == 0)
            cui_iso_box(sc, -25.0f, 0.0f, 30.0f, 60.0f, 60.0f, 40.0f, 6.0f,
                        CUI_RGB(0x3377cc), CUI_RGB(0x224489));
        else
            cui_iso_box(sc, 25.0f, 0.0f, 30.0f, 60.0f, 60.0f, 40.0f, 6.0f,
                        CUI_RGB(0xcc7733), CUI_RGB(0x883318));
    }

    cui_iso_end(sc, ui);
    cui_end_frame(ui);

    const cui_draw_data *dd = cui_get_draw_data(ui);
    size_t bytes = (size_t)dd->width_px * dd->height_px * 4;
    unsigned char *px = malloc(bytes);
    cui_sw_render(sw, dd, px, dd->width_px, dd->height_px, dd->width_px * 4,
                  CUI_SW_RGBA);

    cui_vec2 p = cui_iso_project(&cam, 0.0f, 0.0f, 30.0f);
    int x = (int)(p.x + 0.5f), y = (int)(p.y + 0.5f);
    unsigned char r = 0, g = 0, b = 0;
    if (x >= 0 && x < (int)dd->width_px && y >= 0 && y < (int)dd->height_px) {
        size_t i = ((size_t)y * (size_t)dd->width_px + (size_t)x) * 4;
        r = px[i]; g = px[i + 1]; b = px[i + 2];
    }
    free(px);
    cui_iso_scene_destroy(sc);
    cui_destroy(ui);
    cui_sw_destroy(sw);

    if (r != 0xcc || g != 0x77 || b != 0x33) {
        fprintf(stderr, "FAIL depth order: the overlap at (%d, %d) is %02x%02x%02x, "
                        "expected the near box's lid cc7733\n", x, y, r, g, b);
        return 0;
    }
    return 1;
}

int main(void) {
    unsigned char *a = shot(0);
    unsigned char *b = shot(1);
    if (!a || !b) { fprintf(stderr, "FAIL render\n"); return 1; }

    size_t bytes = (size_t)W * H * 4;
    size_t diff = 0;
    for (size_t i = 0; i < bytes; i++) if (a[i] != b[i]) diff++;
    free(a);
    free(b);

    if (diff != 0) {
        fprintf(stderr, "FAIL sort: %zu bytes differ between forward and reversed order\n", diff);
        return 1;
    }
    printf("ok   sort: insertion order does not change pixels\n");

    if (!depth_order(0) || !depth_order(1)) return 1;
    printf("ok   depth order: the nearer box is painted on top\n");
    return 0;
}
