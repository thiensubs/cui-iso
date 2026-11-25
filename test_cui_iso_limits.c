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

/* A cylinder's side count is clamped to 8..64 by the library, so a request
 * past the cap must draw the same pixels as the cap itself. The count rides in
 * a float field and the draw pass casts it back to an int, so an unclamped
 * 1000 would be drawn as a 1000-gon and the shapes would not match. */
static unsigned char *shot_cylinder(int sides) {
    cui_sw_renderer *s = cui_sw_create();
    cui_ctx *u = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(s),
                                             .theme = CUI_THEME_DARK });
    cui_set_viewport(u, 200, 200, 1.0f);
    cui_iso_scene *scn = cui_iso_scene_create();
    cui_iso_camera cam = { 0.55f, 100, 100, 1.0f };

    cui_begin_frame(u, 1.0f / 60.0f);
    cui_draw_rect(u, (cui_rect){ 0, 0, 200, 200 }, CUI_CORNERS(0),
                  CUI_RGB(0x000000));
    cui_iso_begin(scn, &cam);
    cui_iso_cylinder(scn, 0, 0, 0, 20.0f, 30.0f, sides,
                     CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
    cui_iso_end(scn, u);
    cui_end_frame(u);

    const cui_draw_data *dd = cui_get_draw_data(u);
    size_t bytes = (size_t)dd->width_px * dd->height_px * 4;
    unsigned char *px = malloc(bytes);
    cui_sw_render(s, dd, px, dd->width_px, dd->height_px, dd->width_px * 4,
                  CUI_SW_RGBA);
    cui_iso_scene_destroy(scn);
    cui_destroy(u);
    cui_sw_destroy(s);
    return px;
}

static int same_pixels(const unsigned char *a, const unsigned char *b, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static int cylinder_clamp(void) {
    size_t bytes = 200u * 200u * 4u;
    unsigned char *cap = shot_cylinder(64);
    unsigned char *past = shot_cylinder(1000);
    unsigned char *low = shot_cylinder(8);
    unsigned char *under = shot_cylinder(0);
    int ok = 1;
    if (!cap || !past || !low || !under) {
        fprintf(stderr, "FAIL cylinder clamp: render returned nothing\n");
        ok = 0;
    } else {
        if (!same_pixels(cap, past, bytes)) {
            fprintf(stderr, "FAIL cylinder clamp: 1000 sides did not match 64\n");
            ok = 0;
        }
        if (!same_pixels(low, under, bytes)) {
            fprintf(stderr, "FAIL cylinder clamp: 0 sides did not match 8\n");
            ok = 0;
        }
    }
    free(cap); free(past); free(low); free(under);
    if (ok) printf("ok   cylinder clamp: 1000 sides match 64, 0 sides match 8\n");
    return ok;
}

/* The lattice links each pair once, so the batch holds the balls plus the
 * distinct pairs it joined: 2 nodes give one link, and a request for more
 * sticks than exist joins every pair exactly once. */
static int lattice_counts(void) {
    static const int nodes[3] = { 2, 4, 24 };
    static const int sticks[3] = { 1, 99, 23 };
    static const int want[3] = { 3, 10, 300 };
    for (int i = 0; i < 3; i++) {
        cui_iso_scene *scn = cui_iso_scene_create();
        cui_iso_camera cam = { 0.55f, 100, 100, 1.0f };
        cui_iso_begin(scn, &cam);
        cui_iso_lattice(scn, 0, 0, 0, nodes[i], sticks[i], 20.0f, 3.0f,
                        CUI_RGB(0xb4cf6f), CUI_RGB(0x6b8148), 7u);
        int got = cui_iso_scene_count(scn);
        cui_iso_scene_destroy(scn);
        if (got != want[i]) {
            fprintf(stderr, "FAIL lattice: %d nodes / %d sticks gave %d solids, "
                            "expected %d\n", nodes[i], sticks[i], got, want[i]);
            return 0;
        }
    }
    printf("ok   lattice: node and link counts are right\n");
    return 1;
}

/* A grid past the cell cap must be refused, not truncated: the caller would
 * otherwise read slots the call never filled. */
static int grid_cap(void) {
    cui_iso_slot *cells = malloc(sizeof(cui_iso_slot) * CUI_ISO_MAX_GRID_CELLS);
    if (!cells) {
        fprintf(stderr, "FAIL grid cap: out of memory\n");
        return 0;
    }
    cui_iso_layout_grid_opts o = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .cell = 10.0f };
    int ok = 1;
    if (cui_iso_layout_grid_ex(cells, 64, 64, &o) != 4096) {
        fprintf(stderr, "FAIL grid cap: 64x64 was refused\n");
        ok = 0;
    }
    if (cui_iso_layout_grid_ex(cells, 4096, 2, &o) != 0) {
        fprintf(stderr, "FAIL grid cap: 4096x2 was not refused\n");
        ok = 0;
    }
    if (cui_iso_layout_grid_ex(cells, -1, 4, &o) != 0) {
        fprintf(stderr, "FAIL grid cap: -1x4 was not refused\n");
        ok = 0;
    }
    if (cui_iso_layout_grid_ex(cells, 0, 0, &o) != 0) {
        fprintf(stderr, "FAIL grid cap: 0x0 was not refused\n");
        ok = 0;
    }
    if (cui_iso_layout_grid_ex(cells, 2, 2049, &o) != 0) {
        fprintf(stderr, "FAIL grid cap: 2x2049 was not refused\n");
        ok = 0;
    }
    free(cells);
    if (ok) printf("ok   grid cap: a grid past the cell cap is refused\n");
    return ok;
}

/* A disk stack must stop at the room left in the batch, and it must not run
 * at all outside a batch: a stack of a million disks used to make a million
 * refused insertions. */
static int disk_stack_capacity(void) {
    cui_iso_scene *scn = cui_iso_scene_create();
    cui_iso_camera cam = { 0.55f, 100, 100, 1.0f };

    /* outside a batch: nothing is accepted */
    cui_iso_disk_stack(scn, 0, 0, 0, 10.0f, 5, 2.0f, 1.0f, 12,
                       CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
    if (cui_iso_scene_count(scn) != 0) {
        fprintf(stderr, "FAIL disk stack: %d solids outside a batch\n",
                cui_iso_scene_count(scn));
        cui_iso_scene_destroy(scn);
        return 0;
    }

    cui_iso_begin(scn, &cam);
    for (int i = 0; i < CUI_ISO_MAX_SOLIDS - 3; i++)
        cui_iso_box(scn, 0, 0, 0, 1, 1, 1, 0,
                    CUI_RGB(0x5c6169), CUI_RGB(0x2c2f33));
    cui_iso_disk_stack(scn, 0, 0, 0, 10.0f, 1000000, 2.0f, 1.0f, 12,
                       CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
    int got = cui_iso_scene_count(scn);
    cui_iso_scene_destroy(scn);
    if (got != CUI_ISO_MAX_SOLIDS) {
        fprintf(stderr, "FAIL disk stack: a nearly full batch ended at %d, "
                        "expected %d\n", got, CUI_ISO_MAX_SOLIDS);
        return 0;
    }
    printf("ok   disk stack: a stack stops at the room left in the batch\n");
    return 1;
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

    int ok = 1;
    ok &= cylinder_clamp();
    ok &= lattice_counts();
    ok &= grid_cap();
    ok &= disk_stack_capacity();

    cui_iso_scene_destroy(sc);
    cui_destroy(ui);
    cui_sw_destroy(sw);
    if (bad == 0 && ok) { printf("all limit checks passed\n"); return 0; }
    return 1;
}
