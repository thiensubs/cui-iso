/* test_cui_iso_limits.c - boundary test, run under ASan/UBSan.
 * The smoke test uses comfortable values, so a buffer one element short still
 * passes. This pushes every bounded loop to its maximum and checks degenerate
 * input; it must stay clean and never abort.
 */
#include "cui_iso.h"
#include "cui_iso_internal.h"
#include "cui_iso_opts.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>

static cui_sw_renderer *sw;
static cui_ctx *ui;
static cui_iso_scene *sc;

static void render(void) {
    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_draw_rect(ui, (cui_rect){0, 0, 200, 200}, CUI_CORNERS(0),
                  CUI_RGB(0x000000));
    cui_iso_camera cam = { 0.55f, 100, 100, 1.0f };
    cui_iso_begin(sc, &cam);
}

static void emit(void) {
    cui_iso_end(sc, ui);
    cui_end_frame(ui);
    const cui_draw_data *dd = cui_get_draw_data(ui);
    unsigned char *px = malloc((size_t)dd->width_px * dd->height_px * 4);
    cui_sw_render(sw, dd, px, dd->width_px, dd->height_px, dd->width_px * 4,
                  CUI_SW_RGBA);
    free(px);
}

int main(void) {
    sw = cui_sw_create();
    ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw),
                                     .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, 200, 200, 1.0f);
    sc = cui_iso_scene_create();

    /* A batch larger than the capacity must be clamped, not overflow: the count
     * stops at the cap and the extra solids are dropped. ASan watches the write. */
    render();
    for (int i = 0; i < 4000; i++)
        cui_iso_box(sc, (float)(i % 40), (float)((i / 40) % 40), 10.0f,
                    5, 5, 5, 1, CUI_RGB(0x5c6169), CUI_RGB(0x2c2f33));
    if (cui_iso_scene_count(sc) != CUI_ISO_MAX_SOLIDS) {
        fprintf(stderr, "FAIL capacity: count %d, expected cap %d\n",
                cui_iso_scene_count(sc), CUI_ISO_MAX_SOLIDS);
        return 1;
    }
    printf("ok   capacity: 4000 boxes clamped to %d\n", CUI_ISO_MAX_SOLIDS);
    emit();

    render();
    cui_iso_cylinder(sc, 0, 0, 0, 20.0f, 30.0f, 64,
                     CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
    emit();

    render();
    cui_iso_lattice(sc, 0, 0, 0, 24, 23, 20.0f, 3.0f,
                    CUI_RGB(0xb4cf6f), CUI_RGB(0x6b8148), 7u);
    emit();

    render();
    cui_iso_slot one[1], two[2], many[8];
    int bad = 0;
    if (cui_iso_layout_row_ex(one, 0, &(cui_iso_layout_row_opts){ .spacing = 10 }) != 0) bad++;
    if (cui_iso_layout_row_ex(one, 1, &(cui_iso_layout_row_opts){ .spacing = 10 }) != 1) bad++;
    if (cui_iso_layout_ring_ex(two, 1, &(cui_iso_layout_ring_opts){ .radius = 10 }) != 1) bad++;
    if (cui_iso_layout_grid_ex(many, 0, 4, &(cui_iso_layout_grid_opts){ .cell = 10 }) != 0) bad++;
    if (cui_iso_layout_grid_ex(many, 2, 2, &(cui_iso_layout_grid_opts){ .cell = 10 }) != 4) bad++;
    emit();

    render();
    cui_iso_cylinder(sc, 0, 0, 0, 0, 0, 8,
                     CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
    emit();

    cui_iso_scene_destroy(sc);
    cui_destroy(ui);
    cui_sw_destroy(sw);
    if (bad == 0) { printf("all limit checks passed\n"); return 0; }
    return 1;
}
