/* test_cui_iso_named.c - the named-field macros must be usable and must draw.
 *
 * Each macro is the spelling CUI itself uses for its components: the short name
 * forwards to the _ex function and, through CUI__OPTS, to the options struct.
 * If a struct field were renamed without the macro, or a field missed, this file
 * would stop compiling, which is the point.
 */
#include "cui_iso_opts.h"
#include "cui_iso_internal.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw),
                                            .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, 240, 200, 1.0f);
    cui_iso_scene *sc = cui_iso_scene_create();
    cui_iso_camera cam = { 0.55f, 120, 110, 1.0f };

    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_iso_begin(sc, &cam);

    /* one call per solid, every argument named */
    cui_iso_slab_named(sc, .cx = 0, .cy = 0, .z_top = 0,
                       .w = 120, .d = 120, .height = 12,
                       .top = CUI_COLOR_SURFACE_MUTED, .side = CUI_COLOR_BORDER_SUBTLE);
    cui_iso_box_named(sc, .cx = -40, .cy = 0, .z_top = 12,
                      .w = 40, .d = 40, .height = 30,
                      .top = CUI_COLOR_SURFACE_RAISED, .side = CUI_COLOR_BORDER);
    cui_iso_sphere_named(sc, .x = 40, .y = 0, .z = 50,
                         .radius = 12, .color = CUI_COLOR_ACCENT);
    cui_iso_link_named(sc, .x1 = -40, .y1 = 0, .z1 = 20,
                       .x2 = 40, .y2 = 0, .z2 = 20,
                       .thickness = 3, .color = CUI_COLOR_SUCCESS, .pulse = 0.5f);
    cui_iso_glass_named(sc, .cx = 40, .cy = 0, .z_bottom = 12,
                        .w = 40, .d = 40, .height = 40, .tint = CUI_COLOR_ACCENT_SUBTLE);
    cui_iso_wedge_named(sc, .cx = 0, .cy = 60, .z_base = 0,
                        .w = 40, .d = 30, .height = 16,
                        .roof = CUI_COLOR_ACCENT, .side = CUI_COLOR_BORDER);
    cui_iso_cylinder_named(sc, .cx = -60, .cy = 60, .z_bottom = 0,
                           .radius = 12, .height = 30, .sides = 20,
                           .top = CUI_COLOR_SURFACE_RAISED, .side = CUI_COLOR_BORDER);
    cui_iso_disk_stack_named(sc, .cx = 60, .cy = 60, .z_bottom = 0,
                             .radius = 12, .count = 3, .disk_h = 6, .gap = 3,
                             .sides = 20, .top = CUI_COLOR_ACCENT, .side = CUI_COLOR_BORDER);
    cui_iso_text_named(sc, .x = 0, .y = -40, .z = 40, .text = "named");
    cui_iso_shadow_named(sc, .cx = 0, .cy = 0, .z = 0,
                         .rx = 60, .ry = 40, .color = (cui_color){ 0, 0, 0, 60 });
    cui_iso_grid_named(sc, .cx = 0, .cy = 0, .z = 0, .cells = 4, .cell = 30,
                       .fill = CUI_COLOR_SURFACE, .line = CUI_COLOR_BORDER_SUBTLE);
    cui_iso_arc_named(sc, .x1 = -50, .y1 = -40, .z1 = 10, .x2 = 50, .y2 = -40, .z2 = 10,
                      .bulge = 20, .thickness = 2, .color = CUI_COLOR_INFO, .pulse = 0.25f);
    cui_iso_link_labeled_named(sc, .x1 = -50, .y1 = 20, .z1 = 10, .x2 = 50, .y2 = 20, .z2 = 10,
                              .thickness = 2, .color = CUI_COLOR_INFO, .pulse = 0.25f,
                              .label = "tok", .value = 3.2f, .decimals = 1);
    cui_iso_lattice_named(sc, .cx = 0, .cy = -60, .z = 30, .nodes = 6, .sticks = 2,
                          .spread = 16, .ball = 3,
                          .ball_color = CUI_COLOR_INFO, .stick_color = CUI_COLOR_BORDER,
                          .seed = 7u);

    cui_iso_end(sc, ui);
    cui_end_frame(ui);
    printf("named macros drew %d solids\n", cui_iso_scene_count(sc));

    cui_iso_scene_destroy(sc);
    cui_destroy(ui);
    cui_sw_destroy(sw);
    return 0;
}
