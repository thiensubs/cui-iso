/* cases.c - the "does the API pass" test: a grid of cells, each rendering ONE
 * cui-iso primitive with a caption naming it. If adding a case is one table row
 * plus a few lines, the library is easy to use; if not, it fails.
 *   cui_iso_cases <out.png> */
#include "cui_iso.h"
#include "cui_iso_internal.h"
#include "cui_iso_opts.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One case = a name + a function that draws that primitive into a scene. The
 * camera is set up by the grid; a case only places its own solid at the origin,
 * so each body below is a handful of lines and reads on its own. */
typedef struct { const char *name; void (*draw)(cui_iso_scene *s); } case_t;
/* The palette the case functions draw with, set once in main from the context
 * they render into, so neutral surfaces follow the theme. The accents stay
 * fixed: a status colour means the same thing in either theme. */
static cui_iso_palette gpal;

static void c_box(cui_iso_scene *s) {
    cui_iso_box_named(s,
                      .cx = 0, .cy = 0, .z_top = 30, .w = 70, .d = 70, .height = 60,
                      .chamfer = 10, .top = gpal.block, .side = gpal.block_side);
}

static void c_slab(cui_iso_scene *s) {
    cui_iso_slab_named(s,
                       .cx = 0, .cy = 0, .z_top = 0, .w = 130, .d = 130, .height = 14,
                       .top = gpal.plate, .side = gpal.plate_side);
}

static void c_cylinder(cui_iso_scene *s) {
    cui_iso_cylinder_named(s,
                           .cx = 0, .cy = 0, .z_bottom = 0, .radius = 38, .height = 78,
                           .sides = 28, .top = CUI_RGB(0x9fd14f), .side = CUI_RGB(0x3f5a24));
}

static void c_sphere(cui_iso_scene *s) {
    cui_iso_sphere_named(s,
                         .x = 0, .y = 0, .z = 34, .radius = 26, .color = CUI_RGB(0x4fa3e0));
}

static void c_disk_stack(cui_iso_scene *s) {
    cui_iso_disk_stack_named(s,
                             .cx = 0, .cy = 0, .z_bottom = 0, .radius = 40, .count = 4,
                             .disk_h = 10, .gap = 5, .sides = 28, .top = CUI_RGB(0x9fd14f),
                             .side = CUI_RGB(0x3f5a24));
}

static void c_glass(cui_iso_scene *s) {
    cui_iso_box_named(s,
                      .cx = 0, .cy = 0, .z_top = 16, .w = 60, .d = 60, .height = 18,
                      .chamfer = 6, .top = gpal.plate, .side = gpal.block_side);
    cui_iso_glass_named(s,
                        .cx = 0, .cy = 0, .z_bottom = 16, .w = 78, .d = 78, .height = 56,
                        .tint = (cui_color){0x34, 0xd1, 0x8c, 36});
}

static void c_lattice(cui_iso_scene *s) {
    cui_iso_lattice(s, 0, 0, 30, 12, 2, 30.0f, 5.0f,
                    CUI_RGB(0xb4cf6f), (cui_color){0x6b, 0x81, 0x48, 220}, 7u);
}

static void c_link(cui_iso_scene *s) {
    cui_iso_sphere_named(s,
                         .x = -48, .y = 0, .z = 30, .radius = 12, .color = CUI_RGB(0x4fa3e0));
    cui_iso_sphere_named(s,
                         .x = 48, .y = 0, .z = 30, .radius = 12, .color = CUI_RGB(0x4fa3e0));
    cui_iso_link_named(s,
                       .x1 = -48, .y1 = 0, .z1 = 30, .x2 = 48, .y2 = 0, .z2 = 30,
                       .thickness = 3.0f, .color = CUI_RGB(0x34d18c), .pulse = 0.5f);
}

static void c_arc(cui_iso_scene *s) {
    cui_iso_sphere_named(s,
                         .x = -50, .y = 0, .z = 20, .radius = 12, .color = CUI_RGB(0x4fa3e0));
    cui_iso_sphere_named(s,
                         .x = 50, .y = 0, .z = 20, .radius = 12, .color = CUI_RGB(0x4fa3e0));
    cui_iso_arc_named(s,
                      .x1 = -50, .y1 = 0, .z1 = 20, .x2 = 50, .y2 = 0, .z2 = 20, .bulge = 44,
                      .thickness = 3.0f, .color = CUI_RGB(0x34d18c), .pulse = 0.5f);
}

static void c_link_labeled(cui_iso_scene *s) {
    cui_iso_sphere_named(s,
                         .x = -50, .y = 0, .z = 30, .radius = 12, .color = CUI_RGB(0x9fd14f));
    cui_iso_sphere_named(s,
                         .x = 50, .y = 0, .z = 30, .radius = 12, .color = CUI_RGB(0x4fa3e0));
    cui_iso_link_labeled(s, -50, 0, 30, 50, 0, 30, 2.6f,
                         CUI_RGB(0x34d18c), 0.5f, "tok/s", 3.2f, 1);
}

static void c_shadow(cui_iso_scene *s) {
    cui_iso_shadow_named(s,
                         .cx = 0, .cy = 0, .z = 0, .rx = 58, .ry = 38,
                         .color = (cui_color){0, 0, 0, 110});
    cui_iso_box_named(s,
                      .cx = 0, .cy = 0, .z_top = 44, .w = 60, .d = 60, .height = 44,
                      .chamfer = 8, .top = gpal.block, .side = gpal.block_side);
}

static void c_text(cui_iso_scene *s) {
    cui_iso_box_named(s,
                      .cx = 0, .cy = 0, .z_top = 24, .w = 64, .d = 64, .height = 24,
                      .chamfer = 7, .top = gpal.block, .side = gpal.block_side);
    cui_iso_text_named(s,
                       .x = 0, .y = 0, .z = 40, .text = "AI Factory",
                       .color = (cui_color){232, 234, 238, 255});
}

static void c_wedge(cui_iso_scene *s) {
    cui_iso_box_named(s,
                      .cx = 0, .cy = 0, .z_top = 30, .w = 80, .d = 70, .height = 30,
                      .chamfer = 4, .top = gpal.plate, .side = gpal.block_side);
    cui_iso_wedge_named(s,
                        .cx = 0, .cy = 0, .z_base = 30, .w = 80, .d = 70, .height = 34,
                        .roof = CUI_RGB(0x9fd14f), .side = CUI_RGB(0x3f5a24));
}

/* ring: a hub-and-spoke, the place callback draws one pod per slot */
static void ring_place(cui_iso_scene *s, int i, float x, float y,
                       float angle_frac, void *user) {
    (void)user;
    cui_iso_box_named(s,
                      .cx = x, .cy = y, .z_top = 12, .w = 36, .d = 36, .height = 12,
                      .chamfer = 4, .top = gpal.plate, .side = gpal.block_side);
    cui_iso_sphere(s, x, y, 20, 7,
                   (i % 2) ? CUI_RGB(0x4fa3e0) : CUI_RGB(0x9fd14f));
}

static void c_ring(cui_iso_scene *s) {
    cui_iso_slab_named(s,
                       .cx = 0, .cy = 0, .z_top = 0, .w = 90, .d = 90, .height = 8,
                       .top = gpal.plate, .side = gpal.plate_side);
    cui_iso_ring(s, 0, 0, 8, 52, 5, 0.25f, true,
                 (cui_color){0x34, 0xd1, 0x8c, 255}, ring_place, NULL);
}

static const case_t CASES[] = {
    { "box",          c_box },
    { "slab",         c_slab },
    { "cylinder",     c_cylinder },
    { "sphere",       c_sphere },
    { "disk_stack",   c_disk_stack },
    { "glass",        c_glass },
    { "lattice",      c_lattice },
    { "link",         c_link },
    { "arc",          c_arc },
    { "link_labeled", c_link_labeled },
    { "shadow",       c_shadow },
    { "text",         c_text },
    { "wedge",        c_wedge },
    { "ring",         c_ring },
};
#define NCASES ((int)(sizeof CASES / sizeof CASES[0]))

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "cases.png";
    const int COLS = 4;
    const float CW = 260.0f, CH = 220.0f;          /* cell size, px */
    int rows = (NCASES + COLS - 1) / COLS;
    float W = (float)COLS * CW, H = (float)rows * CH;

    cui_sw_renderer *sw = cui_sw_create();
    bool light = argc > 2 && strcmp(argv[2], "light") == 0;
    cui_ctx *ui = cui_create(&(cui_ctx_desc){
        .renderer = cui_sw_iface(sw),
        .theme = light ? CUI_THEME_LIGHT : CUI_THEME_DARK });
    cui_set_viewport(ui, W, H, 1.0f);
    cui_iso_scene *sc = cui_iso_scene_create();
    cui_iso_palette pal = cui_iso_palette_default(ui);
    gpal = pal;
    cui_iso_text_style ts = { .font = CUI_FONT_MEDIUM, .size = 13.0f };
    ts.color = pal.ink;
    ts.value_color = pal.ink_dim;
    cui_iso_set_text_style(sc, ts);

    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_draw_rect(ui, (cui_rect){0, 0, (int)W, (int)H}, CUI_CORNERS(0),
                  cui_color_of(ui, CUI_COLOR_BACKGROUND));

    for (int i = 0; i < NCASES; i++) {
        int col = i % COLS, row = i / COLS;
        float ox = (float)col * CW, oy = (float)row * CH;

        /* cell background + caption strip */
        cui_draw_rect(ui, (cui_rect){(int)ox + 8, (int)oy + 8, (int)CW - 16, (int)CH - 16},
                      CUI_CORNERS(0), cui_color_of(ui, CUI_COLOR_SURFACE));
        cui_draw_text(ui, CUI_FONT_MEDIUM, 13.0f,
                      (cui_vec2){ ox + 18, oy + CH - 30 }, ts.color,
                      CASES[i].name, (int)strlen(CASES[i].name));

        /* the case: one camera aimed at this cell's centre, then its one call */
        cui_iso_camera cam = { 0.55f, ox + CW * 0.5f, oy + CH * 0.52f, 0.9f };
        cui_iso_begin(sc, &cam);
        CASES[i].draw(sc);
        cui_iso_end(sc, ui);
    }
    cui_end_frame(ui);

    const cui_draw_data *dd = cui_get_draw_data(ui);
    unsigned char *px = malloc((size_t)dd->width_px * dd->height_px * 4);
    cui_sw_render(sw, dd, px, dd->width_px, dd->height_px, dd->width_px * 4, CUI_SW_RGBA);
    bool ok = cui_write_png(path, px, dd->width_px, dd->height_px, dd->width_px * 4);
    printf("%s %s (%d cases)\n", ok ? "wrote" : "FAILED", path, NCASES);

    free(px);
    cui_iso_scene_destroy(sc);
    cui_destroy(ui);
    cui_sw_destroy(sw);
    return ok ? 0 : 1;
}