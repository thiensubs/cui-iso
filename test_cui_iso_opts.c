/* test_cui_iso_opts.c - the named-field forms must draw exactly what the
 * positional forms draw: same solids, byte-identical pixels. This keeps the
 * convenience layer honest; a forwarder that forgot an argument would differ.
 */
#include "cui_iso_opts.h"
#include "cui_iso_internal.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 240
#define H 200

static unsigned char *shot(int named) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw),
                                             .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, W, H, 1.0f);
    cui_iso_scene *sc = cui_iso_scene_create();
    cui_iso_camera cam = { 0.55f, W * 0.5f, H * 0.55f, 1.0f };

    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_draw_rect(ui, (cui_rect){ 0, 0, W, H }, CUI_CORNERS(0),
                  CUI_RGB(0x000000));
    cui_iso_begin(sc, &cam);

    cui_color top = CUI_RGB(0x5c6169), side = CUI_RGB(0x2c2f33);
    cui_color green = CUI_RGB(0x34d18c), blue = CUI_RGB(0x4fa3e0);
    if (named) {
        cui_iso_box_ex(sc, &(cui_iso_box_opts){ .cx = -60, .cy = 0, .z_top = 30,
                                                .w = 40, .d = 40, .height = 30,
                                                .chamfer = 6,
                                                .top = top, .side = side });
        cui_iso_cylinder_ex(sc, &(cui_iso_cylinder_opts){ .cx = 60, .cy = 0,
                                    .z_bottom = 0, .radius = 20, .height = 50,
                                    .sides = 24, .top = green, .side = side });
        cui_iso_sphere_ex(sc, &(cui_iso_sphere_opts){ .x = 0, .y = 0, .z = 70,
                                    .radius = 14, .color = blue });
        cui_iso_link_ex(sc, &(cui_iso_link_opts){ .x1 = -40, .y1 = 0, .z1 = 10,
                                    .x2 = 40, .y2 = 0, .z2 = 10, .thickness = 3,
                                    .pulse = 0.5f, .color = green });
    } else {
        cui_iso_box(sc, -60, 0, 30, 40, 40, 30, 6, top, side);
        cui_iso_cylinder(sc, 60, 0, 0, 20, 50, 24, green, side);
        cui_iso_sphere(sc, 0, 0, 70, 14, blue);
        cui_iso_link(sc, -40, 0, 10, 40, 0, 10, 3, green, 0.5f);
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

int main(void) {
    unsigned char *a = shot(0), *b = shot(1);
    if (!a || !b) { fprintf(stderr, "FAIL render\n"); return 1; }
    size_t bytes = (size_t)W * H * 4, diff = 0;
    for (size_t i = 0; i < bytes; i++) if (a[i] != b[i]) diff++;
    free(a); free(b);
    if (diff == 0) {
        printf("ok   opts: named-field forms match positional, byte for byte\n");
        return 0;
    }
    fprintf(stderr, "FAIL opts: %zu bytes differ\n", diff);
    return 1;
}
