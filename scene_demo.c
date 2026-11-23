// scene_demo.c - the mcpd hub-and-pods diorama rebuilt on the cui_iso layer.
//
// Fake static pods (no mcpd daemon), drag to orbit, click a platform to lift it.
#include <cui/cui.h>
#include <cui/cui_app.h>
#include "cui_iso.h"
#include "cui_iso_internal.h"
#include "cui_iso_opts.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static cui_color rgba(uint32_t rgb, uint8_t a) {
    return (cui_color){ (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb, a };
}

#define COL_BG         CUI_RGB(0x0d0e12)
#define COL_PLAT_TOP   CUI_RGB(0x3d4046)
#define COL_PLAT_SIDE  CUI_RGB(0x232527)
#define COL_TOWER_TOP  CUI_RGB(0x5c6169)
#define COL_TOWER_SIDE CUI_RGB(0x3a3d42)
#define COL_METAL      CUI_RGB(0x918e8b)
#define COL_TEXT       CUI_RGB(0xe8eaee)
#define COL_TEXT_DIM   CUI_RGB(0x8b9096)

#define POD_COUNT 5
#define GLASS_H   42.0f
#define PROP_H    8.0f
#define LIFT_MAX  9.0f

typedef struct {
    const char *name;
    float x, y, w, d;
    int mb;
} pod_def;

static const pod_def PODS[POD_COUNT] = {
    { "weather", -219.5f,  -71.0f, 74, 74, 312 },
    { "search",   -67.6f,  218.5f, 78, 70, 540 },
    { "files",    176.8f,  134.9f, 82, 74, 1204 },
    { "calendar", 176.8f, -134.9f, 70, 70, 186 },
    { "notes",    -67.6f, -218.5f, 76, 76, 264 },
};

typedef struct {
    cui_iso_camera cam;          /* window-space camera, updated by paint */
    float az_target;
    int dragging, selected, hover;
    float last_x, down_x, down_y, moved;
    float mx, my;                /* window-space mouse, for hit tests */
    float t, lift[POD_COUNT];
    cui_iso_scene *scene;
} demo;

static void path_lens(const cui_vec2 *p, int n, float *lens) {
    lens[0] = 0.0f;
    for (int k = 1; k < n; k++) {
        float dx = p[k].x - p[k - 1].x, dy = p[k].y - p[k - 1].y;
        lens[k] = lens[k - 1] + sqrtf(dx * dx + dy * dy);
    }
}

/* A point at parameter t (0..1) along a screen polyline with cumulative lens. */
static cui_vec2 path_point(const cui_vec2 *p, const float *lens, int n, float t) {
    float total = lens[n - 1];
    if (total <= 0.0f) return p[0];
    float want = t * total;
    for (int i = 1; i < n; i++) {
        if (want <= lens[i]) {
            float seg = lens[i] - lens[i - 1];
            float f = seg > 0.0f ? (want - lens[i - 1]) / seg : 0.0f;
            return (cui_vec2){ p[i - 1].x + (p[i].x - p[i - 1].x) * f,
                               p[i - 1].y + (p[i].y - p[i - 1].y) * f };
        }
    }
    return p[n - 1];
}

static void draw_conduit(cui_ctx *ui, const cui_vec2 *p, int n, float bright) {
    cui_draw_polyline(ui, p, n, 22.0f, rgba(0xafca4c, (uint8_t)(22.0f * bright)), false);
    cui_draw_polyline(ui, p, n, 11.0f, rgba(0x2e3a16, (uint8_t)(215 + 40 * bright)), false);
    cui_draw_polyline(ui, p, n, 7.5f, rgba(0x6b8148, (uint8_t)(205 + 50 * bright)), false);
    cui_draw_polyline(ui, p, n, 2.8f, rgba(0xb4cf6f, (uint8_t)(225 + 30 * bright)), false);
    for (int i = 1; i < n - 1; i++)
        cui_draw_circle(ui, p[i], 5.5f, rgba(0x2e3a16, 235));
}

static void draw_pulse(cui_ctx *ui, const cui_vec2 *p, int n, float t) {
    float lens[4];
    path_lens(p, n, lens);
    cui_vec2 c = path_point(p, lens, n, t);
    cui_draw_circle(ui, c, 14.0f, rgba(0xafca4c, 60));
    cui_draw_circle(ui, c, 7.5f, rgba(0xd8f0a0, 140));
    cui_draw_circle(ui, c, 3.4f, rgba(0xf4ffd0, 255));
}

static void draw_molecule(cui_ctx *ui, const cui_iso_camera *cam, float cx, float cy,
                          float z, float scale, int seed) {
    static const cui_color cols[5] = {
        CUI_RGB(0x9fd14f), CUI_RGB(0x8b5cf6), CUI_RGB(0xf5a524),
        CUI_RGB(0xe8eaee), CUI_RGB(0x4fa3e0),
    };
    float px[5], py[5], pz[5];
    uint32_t s = (uint32_t)seed * 2654435761u + 12345u;
    for (int i = 0; i < 5; i++) {
        s = s * 1664525u + 1013904223u;
        px[i] = (float)((int)((s >> 8) % 1000) - 500) / 500.0f * 26.0f * scale;
        s = s * 1664525u + 1013904223u;
        py[i] = (float)((int)((s >> 8) % 1000) - 500) / 500.0f * 26.0f * scale;
        s = s * 1664525u + 1013904223u;
        pz[i] = (float)((int)((s >> 8) % 1000)) / 1000.0f * 18.0f * scale;
    }
    for (int i = 1; i < 5; i++)
        cui_draw_line(ui, cui_iso_project(cam, cx + px[0], cy + py[0], z + pz[0]),
                      cui_iso_project(cam, cx + px[i], cy + py[i], z + pz[i]),
                      1.8f * scale, rgba(0xc9ccd2, 185));
    int order[5] = { 0, 1, 2, 3, 4 };
    for (int i = 1; i < 5; i++) {
        int cur = order[i];
        float cd = cui_iso_depth(cam, cx + px[cur], cy + py[cur]);
        int j = i - 1;
        while (j >= 0 && cui_iso_depth(cam, cx + px[order[j]], cy + py[order[j]]) > cd) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = cur;
    }
    for (int k = 0; k < 5; k++) {
        int i = order[k];
        cui_vec2 c = cui_iso_project(cam, cx + px[i], cy + py[i], z + pz[i]);
        float r = (i == 0 ? 7.4f : 5.5f) * scale;
        cui_draw_circle(ui, c, r, cols[i]);
        cui_draw_circle(ui, (cui_vec2){ c.x - r * 0.28f, c.y - r * 0.30f }, r * 0.42f,
                        rgba(0xffffff, 145));
    }
}

static void centered_text(cui_ctx *ui, cui_font font, float size, cui_vec2 pos,
                          cui_color color, const char *text) {
    int len = (int)strlen(text);
    cui_text_metrics m = cui_measure_text(ui, font, size, text, len);
    cui_draw_text(ui, font, size, (cui_vec2){ pos.x - m.width * 0.5f, pos.y - m.height * 0.5f },
                  color, text, len);
}

static void paint(cui_ctx *ui, cui_rect rect, void *user) {
    demo *d = user;
    const cui_iso_camera *cam = &d->cam;

    cui_draw_rect(ui, rect, CUI_CORNERS(0), COL_BG);

    cui_iso_scene_reset(d->scene);
    cui_iso_begin(d->scene, cam);

    /* ambient glow behind everything */
    cui_vec2 gc = cui_iso_project(cam, 0, 0, 0);
    cui_vec2 glow[24];
    for (int i = 0; i < 24; i++) {
        float a = (float)i / 24.0f * 6.2831853f;
        glow[i] = (cui_vec2){ gc.x + cosf(a) * 300.0f * cam->scale,
                              gc.y + 40.0f * cam->scale + sinf(a) * 170.0f * cam->scale };
    }
    cui_draw_polygon(ui, glow, 24, rgba(0x1a2012, 120));

    for (int i = 0; i < POD_COUNT; i++) {
        float lift = d->lift[i];
        float sc = (PODS[i].w * 0.62f + lift * 8.0f) * cam->scale;
        float sr = (PODS[i].d * 0.40f + lift * 5.0f) * cam->scale;
        uint8_t sa = (uint8_t)(92 - lift * 22);
        cui_iso_shadow(d->scene, PODS[i].x, PODS[i].y, -24.0f, sc, sr,
                       rgba(0x000000, sa));
    }
    cui_iso_shadow(d->scene, 0, 0, -24.0f, 118.0f * cam->scale, 74.0f * cam->scale,
                   rgba(0x000000, 92));

    for (int i = 0; i < POD_COUNT; i++) {
        const pod_def *pd = &PODS[i];
        float lift = d->lift[i];
        cui_color top = d->hover == i ? rgba(0x4a4e55, 255) : COL_PLAT_TOP;
        cui_color side = d->hover == i ? rgba(0x2b2d31, 255) : COL_PLAT_SIDE;
        cui_iso_slab(d->scene, pd->x, pd->y, 16.0f, pd->w, pd->d, 16, top, side);
        cui_iso_glass(d->scene, pd->x, pd->y, 16.0f + lift, 74.0f, 74.0f, GLASS_H,
                      rgba(0xafca4c, 32));
    }

    cui_iso_slab(d->scene, 0, 0, 18.0f, 200, 200, 18, COL_PLAT_TOP, COL_PLAT_SIDE);
    cui_iso_box(d->scene, 0, 0, 48.0f, 92, 92, 30, 8, rgba(0x4e5259, 255), rgba(0x2e3033, 255));
    cui_iso_box(d->scene, 0, 0, 76.0f, 66, 66, 28, 7, rgba(0x565a61, 255), rgba(0x32343a, 255));
    cui_iso_box(d->scene, 0, 0, 114.0f, 42, 42, 38, 6, COL_TOWER_TOP, COL_TOWER_SIDE);

    cui_iso_end(d->scene, ui);

    /* everything below is drawn by hand: band, lamp, conduits, props, labels */
    cui_vec2 band[5] = {
        cui_iso_project(cam, -21, -21, 152), cui_iso_project(cam, 21, -21, 152),
        cui_iso_project(cam, 21, 21, 152), cui_iso_project(cam, -21, 21, 152),
        cui_iso_project(cam, -21, -21, 152),
    };
    cui_draw_polyline(ui, band, 5, 1.8f, rgba(0xb4cf6f, 205), false);

    cui_vec2 lamp = cui_iso_project(cam, 0, 0, 152);
    cui_draw_circle(ui, lamp, 11.0f, rgba(0xafca4c, 55));
    cui_draw_circle(ui, lamp, 5.5f, COL_METAL);
    cui_draw_circle(ui, lamp, 2.2f, rgba(0xf4ffd0, 230));

    for (int i = 0; i < POD_COUNT; i++) {
        const pod_def *pd = &PODS[i];
        float lift = d->lift[i];
        float prop_top = 16.0f + lift + PROP_H + 22.0f;
        cui_vec2 p[4] = {
            cui_iso_project(cam, 0, 0, 114),
            cui_iso_project(cam, 0, 0, 138),
            cui_iso_project(cam, pd->x * 0.55f, pd->y * 0.55f, 138),
            cui_iso_project(cam, pd->x, pd->y, prop_top),
        };
        float bright = d->selected == i ? 1.0f : 0.6f;
        draw_conduit(ui, p, 4, bright);
        draw_pulse(ui, p, 4, fmodf(d->t * 0.35f + (float)i / POD_COUNT, 1.0f));
        draw_molecule(ui, cam, pd->x, pd->y, 16.0f + lift + PROP_H, 1.05f, i + 1);
    }

    for (int i = 0; i < POD_COUNT; i++) {
        const pod_def *pd = &PODS[i];
        float prop_top = 16.0f + d->lift[i] + PROP_H + 22.0f;
        cui_vec2 c = cui_iso_project(cam, pd->x, pd->y, prop_top + 26);
        centered_text(ui, CUI_FONT_MEDIUM, 13.0f, c,
                      d->selected == i ? COL_TEXT : COL_TEXT_DIM, pd->name);
        char buf[48];
        snprintf(buf, sizeof buf, "%d MB", pd->mb);
        cui_vec2 b = cui_iso_project(cam, pd->x, pd->y, -34.0f);
        centered_text(ui, CUI_FONT_BODY, 11.5f, b, COL_TEXT_DIM, buf);
    }
}

static void frame(cui_ctx *ui, void *user) {
    demo *d = user;
    d->t += 1.0f / 60.0f;
    d->cam.az += (d->az_target - d->cam.az) * 0.18f;
    for (int i = 0; i < POD_COUNT; i++) {
        float want = d->selected == i ? LIFT_MAX : 0.0f;
        d->lift[i] += (want - d->lift[i]) * 0.18f;
    }

    cui_vec2 vp = cui_viewport_size(ui);
    d->cam.cx = vp.x * 0.5f;
    d->cam.cy = vp.y * 0.54f;
    d->cam.scale = fminf(vp.x / 760.0f, vp.y / 620.0f);

    cui_canvas_result res = cui_canvas(ui, "scene", CUI_FILL, CUI_FILL, paint, d, sizeof *d);
    d->mx = res.mouse.x + res.rect.x;
    d->my = res.mouse.y + res.rect.y;
    d->hover = -1;
    for (int i = POD_COUNT - 1; i >= 0; i--)
        if (cui_iso_hit_box(&d->cam, d->mx, d->my, PODS[i].x, PODS[i].y,
                            16.0f, PODS[i].w, PODS[i].d)) { d->hover = i; break; }

    if (res.pressed) {
        if (!d->dragging) {
            d->dragging = true;
            d->moved = 0.0f;
            d->down_x = d->mx;
            d->down_y = d->my;
        } else {
            d->az_target -= (d->mx - d->last_x) * 0.006f;
            d->moved += fabsf(d->mx - d->last_x);
        }
        d->last_x = d->mx;
    } else {
        if (d->dragging && d->moved < 3.0f) {
            d->selected = -1;
            for (int i = POD_COUNT - 1; i >= 0; i--)
                if (cui_iso_hit_box(&d->cam, d->down_x, d->down_y, PODS[i].x, PODS[i].y,
                                    16.0f, PODS[i].w, PODS[i].d)) { d->selected = i; break; }
        }
        d->dragging = false;
    }
}

int main(void) {
    static demo d;
    d.cam = (cui_iso_camera){ 0.0f, 0, 0, 1.0f };
    d.az_target = 0.0f;
    d.selected = -1;
    d.hover = -1;
    d.scene = cui_iso_scene_create();
    if (!d.scene) return 1;
    int rc = cui_app_run(&(cui_app_desc){ .title = "mcpd diorama", .width = 900,
                                          .height = 680, .continuous = true },
                         frame, &d);
    cui_iso_scene_destroy(d.scene);
    return rc;
}
