/* test_cui_iso.c - a CTest smoke test. Renders every primitive headlessly and
 * asserts two things per case: it does not crash, and it changes pixels (a
 * primitive that draws nothing, or writes outside the buffer, fails here).
 * Not a pixel-golden test — those belong in the component-shots harness — just
 * a gate that the whole API runs and produces output. */
#include "cui_iso.h"
#include "cui_iso_internal.h"
#include "cui_iso_opts.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

/* Render one case into a fresh buffer and return how many pixels differ from
 * the background colour. A healthy primitive changes a good number of them. */
static long render_count_changed(void (*draw)(cui_iso_scene *), int W, int H) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){
        .renderer = cui_sw_iface(sw), .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, (float)W, (float)H, 1.0f);
    cui_iso_scene *sc = cui_iso_scene_create();

    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_iso_camera cam = { 0.55f, W * 0.5f, H * 0.5f, 1.0f };
    cui_iso_begin(sc, &cam);
    cui_draw_rect(ui, (cui_rect){0, 0, W, H}, CUI_CORNERS(0), CUI_RGB(0x000000));
    draw(sc);
    cui_iso_end(sc, ui);
    cui_end_frame(ui);

    const cui_draw_data *dd = cui_get_draw_data(ui);
    long changed = -1;
    if (dd && dd->width_px > 0 && dd->height_px > 0) {
        unsigned char *px = malloc((size_t)dd->width_px * dd->height_px * 4);
        cui_sw_render(sw, dd, px, dd->width_px, dd->height_px, dd->width_px * 4,
                      CUI_SW_RGBA);
        changed = 0;
        for (long i = 0; i < (long)dd->width_px * dd->height_px; i++)
            if (px[i * 4] != 0 || px[i * 4 + 1] != 0 || px[i * 4 + 2] != 0) changed++;
        free(px);
    } else {
        changed = -1;   /* no draw data at all is itself a failure */
    }

    cui_iso_scene_destroy(sc);
    cui_destroy(ui);
    cui_sw_destroy(sw);
    return changed;
}

static void check(const char *name, void (*draw)(cui_iso_scene *)) {
    long changed = render_count_changed(draw, 200, 200);
    /* a real solid changes at least a few hundred pixels at 200x200 */
    if (changed < 100) {
        fprintf(stderr, "FAIL %s: only %ld pixels changed (expected >= 100)\n",
                name, changed);
        failures++;
    } else {
        printf("ok   %s (%ld px)\n", name, changed);
    }
}

/* one draw function per primitive, each placing a solid near the origin */
static void d_box(cui_iso_scene *s) {
    cui_iso_box(s, 0, 0, 30, 60, 60, 50, 8, CUI_RGB(0x5c6169), CUI_RGB(0x2c2f33));
}
static void d_slab(cui_iso_scene *s) {
    cui_iso_slab(s, 0, 0, 0, 120, 120, 12, CUI_RGB(0x3d4046), CUI_RGB(0x232527));
}
static void d_cylinder(cui_iso_scene *s) {
    cui_iso_cylinder(s, 0, 0, 0, 30, 70, 24, CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
}
static void d_sphere(cui_iso_scene *s) { cui_iso_sphere(s, 0, 0, 30, 22, CUI_RGB(0x4fa3e0)); }
static void d_disk_stack(cui_iso_scene *s) {
    cui_iso_disk_stack(s, 0, 0, 0, 34, 4, 9, 5, 24, CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
}
static void d_glass(cui_iso_scene *s) {
    cui_iso_glass(s, 0, 0, 0, 70, 70, 50, (cui_color){0x34, 0xd1, 0x8c, 60});
}
static void d_lattice(cui_iso_scene *s) {
    cui_iso_lattice(s, 0, 0, 30, 10, 2, 26.0f, 4.0f, CUI_RGB(0xb4cf6f),
                    (cui_color){0x6b, 0x81, 0x48, 220}, 7u);
}
static void d_link(cui_iso_scene *s) {
    cui_iso_link(s, -40, 0, 30, 40, 0, 30, 3.0f, CUI_RGB(0x34d18c), 0.5f);
}
static void d_arc(cui_iso_scene *s) {
    cui_iso_arc(s, -40, 0, 20, 40, 0, 20, 40, 3.0f, CUI_RGB(0x34d18c), 0.5f);
}
static void d_link_labeled(cui_iso_scene *s) {
    cui_iso_link_labeled(s, -40, 0, 30, 40, 0, 30, 2.6f, CUI_RGB(0x34d18c), 0.5f,
                         "tok/s", 3.2f, 1);
}
static void d_shadow(cui_iso_scene *s) {
    cui_iso_shadow(s, 0, 0, 0, 50, 32, (cui_color){255, 255, 255, 200});
}
static void d_grid(cui_iso_scene *s) {
    cui_iso_grid(s, 0, 0, 0, 6, 20, CUI_RGB(0x181d27), CUI_RGB(0x27303f));
}
static void d_text(cui_iso_scene *s) {
    cui_iso_text(s, 0, 0, 30, "hello", (cui_color){232, 234, 238, 255});
}
static void d_wedge(cui_iso_scene *s) {
    cui_iso_wedge(s, 0, 0, 0, 70, 60, 30, CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
}

int main(void) {
    /* reduced motion must also work without crashing */
    check("box", d_box);
    check("slab", d_slab);
    check("cylinder", d_cylinder);
    check("sphere", d_sphere);
    check("disk_stack", d_disk_stack);
    check("glass", d_glass);
    check("lattice", d_lattice);
    check("link", d_link);
    check("arc", d_arc);
    check("link_labeled", d_link_labeled);
    check("shadow", d_shadow);
    check("grid", d_grid);
    check("text", d_text);
    check("wedge", d_wedge);

    /* picking: the box top face is at z=30, so aim at its *projected* centre,
     * not the raw camera centre (which is world origin at z=0). */
    {
        cui_iso_camera cam = { 0.55f, 100.0f, 100.0f, 1.0f };
        cui_vec2 ctr = cui_iso_project(&cam, 0, 0, 30);
        bool in = cui_iso_hit_box(&cam, ctr.x, ctr.y, 0, 0, 30, 60, 60);
        bool out = cui_iso_hit_box(&cam, 5.0f, 5.0f, 0, 0, 30, 60, 60);
        if (!in) { fprintf(stderr, "FAIL pick: centre of box top not hit\n"); failures++; }
        if (out) { fprintf(stderr, "FAIL pick: point far outside box hit\n"); failures++; }
        if (in && !out) printf("ok   picking\n");
    }

    if (failures == 0) {
        printf("all cui-iso smoke checks passed\n");
        return 0;
    }
    fprintf(stderr, "%d cui-iso smoke check(s) failed\n", failures);
    return 1;
}