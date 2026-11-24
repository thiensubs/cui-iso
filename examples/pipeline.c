/* pipeline.c - a CI/CD pipeline as an isometric diorama.
 *
 * A complete cui-iso example in the shape CUI's own examples use: a frame
 * function handed to cui_app_run, drawing into a cui_canvas. Seven stages run
 * left to right along a conveyor; each is a platform holding a status marker,
 * and each arrow between stages carries that stage's duration. Colour is
 * semantic — green passed, amber running, red failed — and always has a text
 * label beside it, never colour alone.
 *
 *   # build (from cui-iso/, after `cmake -S . -B build && cmake --build build`)
 *   ./build/pipeline
 *
 * Drag to orbit, click a stage to select it. Everything is drawn with plain
 * cui_draw_* calls through cui-iso; no WebGL, no assets.
 */
#include "cui_iso.h"
#include "cui_iso_opts.h"
#include <cui/cui_app.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- the pipeline data ---------------------------------------------------- */

typedef enum { ST_DONE, ST_RUNNING, ST_FAILED, ST_PENDING } stage_state;

typedef struct {
    const char *name;
    const char *detail;      /* shown under the stage name */
    float seconds;           /* how long this stage took / is taking */
    stage_state state;
} stage_t;

static stage_t STAGES[] = {
    { "Checkout", "main @ 4f2a91c",  8.0f,  ST_DONE    },
    { "Lint",     "clang-tidy",      21.0f, ST_DONE    },
    { "Build",    "Release, 412 TU", 96.0f, ST_DONE    },
    { "Unit",     "1,284 tests",     47.0f, ST_RUNNING },
    { "Package",  "tar + sign",      33.0f, ST_PENDING },
    { "Deploy",   "canary 5%",       0.0f,  ST_PENDING },
    { "Smoke",    "18 checks",       0.0f,  ST_PENDING },
};
#define NSTAGES ((int)(sizeof STAGES / sizeof STAGES[0]))

/* Semantic status colours — status means status, and always has a label. */
static cui_color state_color(stage_state st) {
    switch (st) {
        case ST_DONE:    return CUI_RGB(0x34d18c);   /* green: passed */
        case ST_RUNNING: return CUI_RGB(0xf5a524);   /* amber: running */
        case ST_FAILED:  return CUI_RGB(0xe5484d);   /* red: failed */
        default:         return CUI_RGB(0x8b9096);   /* queued: neutral */
    }
}
static const char *state_label(stage_state st) {
    switch (st) {
        case ST_DONE:    return "passed";
        case ST_RUNNING: return "running";
        case ST_FAILED:  return "failed";
        default:         return "queued";
    }
}

/* ---- layout --------------------------------------------------------------- */

#define STAGE_GAP 104.0f      /* world units between stage centres */

typedef struct {
    cui_iso_scene *scene;
    cui_iso_camera cam;
    float az_target;
    float t;
    float mx, my, last_x, down_x, down_y;
    bool dragging;
    float moved;
    int hover, selected;      /* stage index under the pointer / clicked */
} app;

/* Stage i sits on a straight conveyor along the x axis. */
static void stage_pos(int i, float *x, float *y) {
    *x = (float)(i - (NSTAGES - 1) / 2.0f) * STAGE_GAP;
    *y = 0.0f;
}

/* All the cui-iso drawing for one frame. Called from inside cui_canvas. */
static void paint(cui_ctx *ui, cui_rect rect, void *user) {
    app *a = user;
    (void)rect;

    cui_iso_begin(a->scene, &a->cam);
    cui_iso_grid_named(a->scene,
                       .cx = 0, .cy = 0, .z = 0, .cells = 16, .cell = 46,
                       .fill = CUI_RGB(0x171a1f), .line = CUI_RGB(0x23272e));

    /* solids: a contact shadow, platform and status marker per stage */
    for (int i = 0; i < NSTAGES; i++) {
        stage_t *st = &STAGES[i];
        float x, y;
        stage_pos(i, &x, &y);
        cui_color accent = state_color(st->state);
        bool active = (i == a->hover || i == a->selected);
        float lift = (st->state == ST_RUNNING) ? 6.0f : 0.0f;

        cui_iso_shadow_named(a->scene,
                             .cx = x, .cy = y, .z = 0.5f, .rx = 36, .ry = 24,
                             .color = (cui_color){0, 0, 0, 80});
        cui_iso_slab_named(a->scene,
                           .cx = x, .cy = y, .z_top = 0, .w = 70, .d = 58, .height = 14,
                           .top = active ? CUI_RGB(0x454a53) : CUI_RGB(0x3a3f47),
                           .side = CUI_RGB(0x23262b));
        cui_iso_sphere_named(a->scene,
                             .x = x, .y = y, .z = 14 + lift,
                             .radius = active ? 9.5f : 8.0f, .color = accent);
    }

    /* connectors carrying each stage's duration */
    for (int i = 0; i < NSTAGES - 1; i++) {
        if (STAGES[i].state == ST_PENDING) continue;
        float x0, y0, x1, y1;
        stage_pos(i, &x0, &y0);
        stage_pos(i + 1, &x1, &y1);
        float pulse = (STAGES[i].state == ST_RUNNING)
                          ? (a->t - floorf(a->t)) : -1.0f;
        cui_iso_link_labeled_named(a->scene,
                                   .x1 = x0 + 26.0f, .y1 = y0, .z1 = 8.0f,
                                   .x2 = x1 - 26.0f, .y2 = y1, .z2 = 8.0f,
                                   .thickness = 2.4f,
                                   .color = state_color(STAGES[i].state),
                                   .pulse = pulse, .label = "",
                                   .value = STAGES[i].seconds, .decimals = 0);
    }
    cui_iso_end(a->scene, ui);

    /* labels float above the solids */
    for (int i = 0; i < NSTAGES; i++) {
        stage_t *st = &STAGES[i];
        float x, y;
        stage_pos(i, &x, &y);
        cui_vec2 top = cui_iso_project(&a->cam, x, y, 34.0f);
        cui_vec2 low = cui_iso_project(&a->cam, x, y, -30.0f);
        cui_color ink = CUI_RGB(0xe8eaee), dim = CUI_RGB(0x8b9096);

        cui_draw_text(ui, CUI_FONT_MEDIUM, 14.0f,
                      (cui_vec2){ top.x - 34.0f, top.y }, ink, st->name,
                      (int)strlen(st->name));
        cui_draw_text(ui, CUI_FONT_BODY, 11.5f,
                      (cui_vec2){ top.x - 34.0f, top.y + 17.0f }, dim,
                      st->detail, (int)strlen(st->detail));
        /* state + duration on one labelled line, so colour is never the only cue */
        char line[48];
        snprintf(line, sizeof line, "%s · %.0fs", state_label(st->state),
                 (double)st->seconds);
        cui_draw_text(ui, CUI_FONT_BODY, 11.5f,
                      (cui_vec2){ low.x - 30.0f, low.y },
                      state_color(st->state), line, (int)strlen(line));
    }
}

static void frame(cui_ctx *ui, void *user) {
    app *a = user;
    a->t += 1.0f / 60.0f;
    a->cam.az += (a->az_target - a->cam.az) * 0.12f;

    cui_vec2 vp = cui_viewport_size(ui);
    a->cam.cx = vp.x * 0.5f;
    a->cam.cy = vp.y * 0.5f;
    a->cam.scale = fminf(vp.x / ((NSTAGES + 1) * STAGE_GAP), vp.y / 380.0f) * 1.05f;

    cui_canvas_result res = cui_canvas(ui, "pipeline", CUI_FILL, CUI_FILL,
                                       paint, a, sizeof *a);
    a->mx = res.mouse.x + res.rect.x;
    a->my = res.mouse.y + res.rect.y;

    /* orbit with drag, select on a click (a press that barely moved) */
    if (res.pressed) {
        if (!a->dragging) {
            a->dragging = true; a->moved = 0.0f;
            a->down_x = a->mx; a->down_y = a->my;
            a->last_x = a->mx;
        } else {
            a->az_target -= (a->mx - a->last_x) * 0.006f;
            a->moved += fabsf(a->mx - a->last_x);
            a->last_x = a->mx;
        }
    } else {
        if (a->dragging && a->moved < 3.0f) {
            a->selected = -1;
            for (int i = NSTAGES - 1; i >= 0; i--) {
                float sx, sy;
                stage_pos(i, &sx, &sy);
                if (cui_iso_hit_cylinder(&a->cam, a->down_x, a->down_y,
                                         sx, sy, 0.0f, 30.0f, 26.0f)) {
                    a->selected = i; break;
                }
            }
        }
        a->dragging = false;
    }

    /* hover the top-most stage under the pointer when not dragging */
    a->hover = -1;
    if (!a->dragging)
        for (int i = NSTAGES - 1; i >= 0; i--) {
            float sx, sy;
            stage_pos(i, &sx, &sy);
            if (cui_iso_hit_cylinder(&a->cam, a->mx, a->my, sx, sy,
                                     0.0f, 30.0f, 26.0f)) { a->hover = i; break; }
        }
}

int main(void) {
    static app a;
    a.cam = (cui_iso_camera){ 0.0f, 0, 0, 1.0f };
    a.az_target = 0.0f;
    a.hover = -1;
    a.selected = -1;
    a.scene = cui_iso_scene_create();
    if (!a.scene) return 1;
    cui_iso_text_style ts = { .font = CUI_FONT_MEDIUM, .size = 13.0f };
    ts.color = CUI_RGB(0xe8eaee);
    ts.value_color = CUI_RGB(0x8b9096);
    cui_iso_set_text_style(a.scene, ts);
    int rc = cui_app_run(&(cui_app_desc){ .title = "CI pipeline",
                                          .width = 980, .height = 420,
                                          .continuous = true },
                         frame, &a);
    cui_iso_scene_destroy(a.scene);
    return rc;
}