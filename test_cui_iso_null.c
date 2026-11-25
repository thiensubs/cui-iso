/* test_cui_iso_null.c - a mis-wired client must draw nothing, not crash.
 * Every public entry point takes a NULL scene and a NULL camera here, and a
 * full scene must not relabel an unrelated solid (row 5 of AUDIT.md).
 */
#include "cui_iso_opts.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The refused-insertion frame. `with_refused` adds one call that cannot fit in
 * the batch; the two frames must come out byte for byte equal.
 *
 * The batch is filled with invisible shadows and the last solid is a caption:
 * a label written onto the wrong solid would change the words on screen, which
 * is the whole point of the check. */
#define NULL_W 200
#define NULL_H 160

static unsigned char *render_refused(int with_refused) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw),
                                             .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, NULL_W, NULL_H, 1.0f);
    cui_iso_scene *sc = cui_iso_scene_create();
    cui_iso_camera cam = { 0.55f, NULL_W * 0.5f, NULL_H * 0.55f, 1.0f };

    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_draw_rect(ui, (cui_rect){ 0, 0, NULL_W, NULL_H }, CUI_CORNERS(0),
                  CUI_RGB(0x000000));
    cui_iso_begin(sc, &cam);

    for (int i = 0; i < CUI_ISO_MAX_SOLIDS - 1; i++)
        cui_iso_shadow_named(sc, .cx = 0.0f, .cy = 0.0f, .z = 0.0f,
                             .rx = 1.0f, .ry = 1.0f,
                             .color = CUI_RGBA(0x00000000));
    cui_iso_text_named(sc, .x = 0.0f, .y = 0.0f, .z = 0.0f, .text = "keep",
                       .color = CUI_RGB(0xffffff));

    if (with_refused)
        cui_iso_link_labeled_named(sc, .x1 = -10.0f, .y1 = 0.0f, .z1 = 0.0f,
                                   .x2 = 10.0f, .y2 = 0.0f, .z2 = 0.0f,
                                   .thickness = 2.0f, .color = CUI_RGB(0xffffff),
                                   .pulse = 0.5f, .label = "leak", .value = 1,
                                   .decimals = 0);
    if (cui_iso_scene_count(sc) != CUI_ISO_MAX_SOLIDS)
        fprintf(stderr, "FAIL inert: batch holds %d solids, expected %d\n",
                cui_iso_scene_count(sc), CUI_ISO_MAX_SOLIDS);

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

int main(void) {
    cui_iso_scene *none = NULL;
    cui_color c = CUI_RGB(0x5c6169);
    cui_iso_light l = cui_iso_light_default();

    /* constructors must survive a NULL scene */
    cui_iso_box_named(none,
                      .cx = 0, .cy = 0, .z_top = 1, .w = 2, .d = 3, .height = 4,
                      .chamfer = 5, .top = c, .side = c);
    cui_iso_slab_named(none,
                       .cx = 0, .cy = 0, .z_top = 1, .w = 2, .d = 3, .height = 4, .top = c,
                       .side = c);
    cui_iso_glass_named(none,
                        .cx = 0, .cy = 0, .z_bottom = 1, .w = 2, .d = 3, .height = 4,
                        .tint = c);
    cui_iso_cylinder_named(none,
                           .cx = 0, .cy = 0, .z_bottom = 1, .radius = 2, .height = 3,
                           .sides = 4, .top = c, .side = c);
    cui_iso_sphere_named(none,
                         .x = 0, .y = 0, .z = 1, .radius = 2, .color = c);
    cui_iso_shadow_named(none,
                         .cx = 0, .cy = 0, .z = 1, .rx = 2, .ry = 3, .color = c);
    cui_iso_grid_named(none,
                       .cx = 0, .cy = 0, .z = 1, .cells = 2, .cell = 3, .fill = c, .line = c);
    cui_iso_link_named(none,
                       .x1 = 0, .y1 = 0, .z1 = 1, .x2 = 2, .y2 = 3, .z2 = 4, .thickness = 5,
                       .color = c, .pulse = 0.5f);
    cui_iso_arc_named(none,
                      .x1 = 0, .y1 = 0, .z1 = 1, .x2 = 2, .y2 = 3, .z2 = 4, .bulge = 5,
                      .thickness = 6, .color = c, .pulse = 0.5f);
    cui_iso_link_labeled_named(none,
                               .x1 = 0, .y1 = 0, .z1 = 1, .x2 = 2, .y2 = 3, .z2 = 4,
                               .thickness = 5, .color = c, .pulse = 0.5f, .label = "x",
                               .value = 1, .decimals = 0);
    cui_iso_lattice_named(none,
                          .cx = 0, .cy = 0, .z = 1, .nodes = 6, .sticks = 2, .spread = 6,
                          .ball = 6, .ball_color = c, .stick_color = c, .seed = 1u);
    cui_iso_disk_stack_named(none,
                             .cx = 0, .cy = 0, .z_bottom = 1, .radius = 2, .count = 3,
                             .disk_h = 4, .gap = 5, .sides = 6, .top = c, .side = c);
    cui_iso_text_named(none,
                       .x = 0, .y = 0, .z = 1, .text = "t", .color = c);
    cui_iso_wedge_named(none,
                        .cx = 0, .cy = 0, .z_base = 1, .w = 2, .d = 3, .height = 4,
                        .roof = c, .side = c);
    cui_iso_set_text_style(none, (cui_iso_text_style){ 0 });
    cui_iso_set_light(none, l);
    cui_iso_set_reduced_motion(none, true);
    cui_iso_set_bounds(none, (cui_rect){ 0, 0, 10, 10 });
    cui_iso_scene_reset(none);
    cui_iso_scene_destroy(none);
    printf("NULL scene handled\n");

    /* begin with a NULL camera, then end: nothing drawn, nothing read */
    cui_iso_scene *sc = cui_iso_scene_create();
    cui_iso_begin(sc, NULL);
    cui_iso_box_named(sc,
                      .cx = 0, .cy = 0, .z_top = 0, .w = 1, .d = 1, .height = 1,
                      .chamfer = 1, .top = c, .side = c);
    printf("NULL camera safe: count %d\n", cui_iso_scene_count(sc));

    /* a full scene must not relabel an older solid
     * (label guard row 5). Fill past the cap, then add a labelled link.
     */
    cui_iso_camera cam = { 0.55f, 100, 100, 1.0f };
    cui_iso_begin(sc, &cam);
    for (int i = 0; i < CUI_ISO_MAX_SOLIDS; i++)
        cui_iso_box_named(sc,
                          .cx = 0, .cy = 0, .z_top = 0, .w = 1, .d = 1, .height = 1,
                          .chamfer = 1, .top = c, .side = c);
    cui_iso_link_labeled_named(sc,
                               .x1 = 0, .y1 = 0, .z1 = 0, .x2 = 10, .y2 = 0, .z2 = 0,
                               .thickness = 2, .color = c, .pulse = 0.5f, .label = "leak",
                               .value = 1, .decimals = 0);
    if (cui_iso_scene_count(sc) != CUI_ISO_MAX_SOLIDS) {
        fprintf(stderr, "FAIL label guard: count %d\n", cui_iso_scene_count(sc));
        return 1;
    }
    printf("ok   label guard: full scene unchanged, count %d\n",
           cui_iso_scene_count(sc));
    cui_iso_scene_destroy(sc);

    /* A refused insertion must leave the batch exactly as it was. The label
     * used to be written onto whatever solid happened to be last, so a link
     * that did not fit relabelled a caption that was already in the batch. */
    unsigned char *refused = render_refused(1);
    unsigned char *plain = render_refused(0);
    if (!refused || !plain) {
        fprintf(stderr, "FAIL inert: render returned nothing\n");
        free(refused);
        free(plain);
        return 1;
    }
    size_t bytes = (size_t)NULL_W * NULL_H * 4;
    size_t diff = 0;
    for (size_t i = 0; i < bytes; i++)
        if (refused[i] != plain[i]) diff++;
    free(refused);
    free(plain);
    if (diff != 0) {
        fprintf(stderr, "FAIL inert: a refused insertion changed %zu bytes\n", diff);
        return 1;
    }
    printf("ok   inert: a refused insertion changed no pixels\n");
    return 0;
}
