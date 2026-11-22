/* Renders a isometric diorama headless with the software renderer and writes a PNG.
 *   cui_iso_render <out.png> [width] [height] [az_degrees] */
#include "cui_iso.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void diorama(cui_ctx *ui, const cui_iso_camera *cam, cui_iso_scene *sc) {
    cui_draw_rect(ui, (cui_rect){0, 0, cam->cx * 2, cam->cy * 2}, CUI_CORNERS(0),
                  cui_color_of(ui, CUI_COLOR_BACKGROUND));

    cui_iso_scene_reset(sc);
    cui_iso_begin(sc, cam);

    cui_iso_shadow(sc, 0, 0, 0, 165, 120, (cui_color){0x34, 0xd1, 0x8c, 18});

    cui_iso_slab(sc, 0, 0, 0, 200, 200, 14, CUI_RGB(0x3d4046), CUI_RGB(0x232527));

    cui_iso_box(sc, 0, 0, 14, 84, 84, 22, 6, CUI_RGB(0x5c6169), CUI_RGB(0x2c2f33));
    cui_iso_box(sc, 0, 0, 36, 58, 58, 22, 6, CUI_RGB(0x4e5259), CUI_RGB(0x2c2f33));
    cui_iso_box(sc, 0, 0, 58, 34, 34, 22, 6, CUI_RGB(0x565a61), CUI_RGB(0x2c2f33));

    struct { float x, y; } pod[3] = { {-70, 55}, {72, 30}, {15, -72} };
    for (int i = 0; i < 3; i++) {
        cui_iso_shadow(sc, pod[i].x, pod[i].y, 0.5f, 28, 20,
                       (cui_color){0, 0, 0, 70});
        cui_iso_box(sc, pod[i].x, pod[i].y, 14, 30, 30, 20, 4,
                    CUI_RGB(0x4a4e55), CUI_RGB(0x2a2d31));
        cui_iso_glass(sc, pod[i].x, pod[i].y, 14, 38, 38, 30,
                      (cui_color){0x34, 0xd1, 0x8c, 35});
    }

    cui_iso_end(sc, ui);
}

/* A flow / pipeline scene: sphere nodes on cylinder pillars, joined by links
 * with a travelling pulse. Exercises CUI_ISO_CYLINDER, CUI_ISO_SPHERE and
 * CUI_ISO_LINK — the "sense flow" primitives. */
static void flow(cui_ctx *ui, const cui_iso_camera *cam, cui_iso_scene *sc, float t) {
    cui_draw_rect(ui, (cui_rect){0, 0, cam->cx * 2, cam->cy * 2}, CUI_CORNERS(0),
                  CUI_RGB(0x12151a));

    cui_iso_scene_reset(sc);
    cui_iso_begin(sc, cam);

    cui_iso_grid(sc, 0, 0, 0, 12, 40,
                 (cui_color){0x1c, 0x24, 0x33, 255}, (cui_color){0x2a, 0x35, 0x48, 255});
    cui_iso_shadow(sc, 0, 0, 0.5f, 190, 130, (cui_color){0x4f, 0xa3, 0xe0, 20});
    cui_iso_slab(sc, 0, 0, 0, 240, 180, 12, CUI_RGB(0x343941), CUI_RGB(0x1f2227));

    /* a source tank (cylinder) and a few node pillars */
    struct { float x, y; } node[4] = { {-80, 40}, {0, 60}, {80, 20}, {10, -50} };
    cui_iso_cylinder(sc, node[0].x, node[0].y, 12, 26, 46, 24,
                     CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
    for (int i = 1; i < 4; i++)
        cui_iso_cylinder(sc, node[i].x, node[i].y, 12, 14, 30, 20,
                         CUI_RGB(0x5c6169), CUI_RGB(0x2a2d31));

    /* links from the tank to each node, pulses staggered by time, with a live
     * value on each — the "LLM --3.2k tok/s--> Optimize" pipeline look. */
    for (int i = 1; i < 4; i++) {
        float p = t - (float)i * 0.18f;
        p = p - floorf(p);                       /* wrap 0..1 */
        float tok = 3.2f - (float)i * 0.4f + 0.3f * sinf(t * 2.0f + (float)i);
        cui_iso_link_labeled(sc, node[0].x, node[0].y, 58,
                             node[i].x, node[i].y, 42, 2.2f,
                             (cui_color){0x34, 0xd1, 0x8c, 255}, p,
                             "tok/s", tok, 1);
    }

    /* sphere node markers on top of each pillar */
    cui_iso_sphere(sc, node[0].x, node[0].y, 58, 9, CUI_RGB(0xb4cf6f));
    for (int i = 1; i < 4; i++)
        cui_iso_sphere(sc, node[i].x, node[i].y, 42, 7, CUI_RGB(0x4fa3e0));

    cui_iso_end(sc, ui);
}

/* A composite showcase: every primitive in one frame, mirroring the recurring
 * motifs in the NVIDIA diorama art. Exercises grid, slab, box, cylinder,
 * disk-stack, glass, sphere, lattice, link, arc, shadow. */
static void showcase(cui_ctx *ui, const cui_iso_camera *cam, cui_iso_scene *sc, float t) {
    cui_draw_rect(ui, (cui_rect){0, 0, cam->cx * 2, cam->cy * 2}, CUI_CORNERS(0),
                  CUI_RGB(0x10131a));

    cui_iso_scene_reset(sc);
    cui_iso_begin(sc, cam);

    /* floor + ambient glow */
    cui_iso_grid(sc, 0, 0, 0, 14, 44,
                 CUI_RGB(0x181d27), CUI_RGB(0x27303f));

    /* central hub: a glass cube over a lattice (the recurring neural-net motif) */
    cui_iso_shadow(sc, 0, 0, 0.5f, 120, 78, (cui_color){0, 0, 0, 90});
    cui_iso_slab(sc, 0, 0, 0, 120, 120, 14, CUI_RGB(0x3a3f47), CUI_RGB(0x22252b));
    cui_iso_lattice(sc, 0, 0, 26, 12, 2, 26.0f, 4.2f,
                    (cui_color){0xb4, 0xcf, 0x6f, 255}, (cui_color){0x6b, 0x81, 0x48, 220},
                    7u);
    cui_iso_glass(sc, 0, 0, 14, 84, 84, 66, (cui_color){0x9f, 0xd1, 0x4f, 34});

    /* left: a database drum stack (disk-stack motif) */
    cui_iso_shadow(sc, -130, 40, 0.5f, 46, 30, (cui_color){0, 0, 0, 80});
    cui_iso_disk_stack(sc, -130, 40, 0, 30, 4, 9, 5, 24,
                       CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));

    /* right: a tall cylinder tank with a sphere marker */
    cui_iso_shadow(sc, 135, -30, 0.5f, 44, 30, (cui_color){0, 0, 0, 80});
    cui_iso_cylinder(sc, 135, -30, 0, 26, 70, 24, CUI_RGB(0x5c6169), CUI_RGB(0x2a2d31));
    cui_iso_sphere(sc, 135, -30, 74, 9, CUI_RGB(0x4fa3e0));

    /* curved arcs from the hub out to the drum and the tank, pulses travelling */
    float pa = t - floorf(t);
    float pb = (t + 0.5f) - floorf(t + 0.5f);
    cui_iso_arc(sc, -46, 28, 30, -112, 44, 40, 46, 2.4f,
                (cui_color){0x34, 0xd1, 0x8c, 255}, pa);
    cui_iso_arc(sc, 46, -20, 30, 124, -26, 74, 40, 2.4f,
                (cui_color){0x4f, 0xa3, 0xe0, 255}, pb);

    /* a couple of satellite pods with glass cases */
    struct { float x, y; } pod[2] = { {90, 110}, {-70, -110} };
    for (int i = 0; i < 2; i++) {
        cui_iso_shadow(sc, pod[i].x, pod[i].y, 0.5f, 34, 24, (cui_color){0, 0, 0, 70});
        cui_iso_box(sc, pod[i].x, pod[i].y, 0, 44, 44, 18, 5,
                    CUI_RGB(0x4a4e55), CUI_RGB(0x2a2d31));
        cui_iso_glass(sc, pod[i].x, pod[i].y, 18, 50, 50, 40,
                      (cui_color){0x34, 0xd1, 0x8c, 32});
    }

    cui_iso_end(sc, ui);
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "cui_iso.png";
    float W = argc > 2 ? (float)atoi(argv[2]) : 760;
    float H = argc > 3 ? (float)atoi(argv[3]) : 620;
    float az = argc > 4 ? (float)(atof(argv[4]) * 3.14159265f / 180.0f) : 0.0f;
    const char *scene = argc > 5 ? argv[5] : "diorama";
    float t = argc > 6 ? (float)atof(argv[6]) : 0.0f;

    cui_iso_camera cam = { az, W * 0.5f, H * 0.56f, 1.0f };

    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){
        .renderer = cui_sw_iface(sw), .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, W, H, 1.0f);

    cui_iso_scene *sc = cui_iso_scene_create();
    cui_begin_frame(ui, 1.0f / 60.0f);
    if (scene[0] == 'f') flow(ui, &cam, sc, t);
    else if (scene[0] == 's') showcase(ui, &cam, sc, t);
    else diorama(ui, &cam, sc);
    cui_end_frame(ui);

    const cui_draw_data *dd = cui_get_draw_data(ui);
    int w = dd->width_px, h = dd->height_px;
    unsigned char *px = malloc((size_t)w * h * 4);
    cui_sw_render(sw, dd, px, w, h, w * 4, CUI_SW_RGBA);
    bool ok = cui_write_png(path, px, w, h, w * 4);
    printf("%s %s\n", ok ? "wrote" : "FAILED", path);

    free(px);
    cui_iso_scene_destroy(sc);
    cui_destroy(ui);
    cui_sw_destroy(sw);
    return ok ? 0 : 1;
}
