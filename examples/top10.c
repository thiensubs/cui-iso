/* top10.c - renders ten real-world cui-iso scenes to PNGs, one per case.
 * Each is a compact diorama built only from the library's primitives, showing
 * the kind of job the layer is good for. Run from cui-iso/:
 *
 *   ./build/top10 /tmp/top10        (writes /tmp/top10/01.png .. 10.png)
 *
 * then compose them with the PIL script in tools/contact_sheet.py.
 */
#include "cui_iso.h"
#include "cui_iso_opts.h"
#include <cui/cui_render.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 360
#define H 260

/* one shared renderer/ctx, re-used for every case */
static cui_sw_renderer *sw;
static cui_ctx *ui;
static cui_iso_scene *sc;

static void begin_case(const char *bg) {
    cui_begin_frame(ui, 1.0f / 60.0f);
    cui_draw_rect(ui, (cui_rect){0, 0, W, H}, CUI_CORNERS(0),
                  cui_color_of(ui, CUI_COLOR_BACKGROUND));
    cui_iso_scene_reset(sc);
    /* a camera framed for a small card */
    cui_iso_camera cam = { 0.55f, W * 0.5f, H * 0.55f, 0.62f };
    cui_iso_begin(sc, &cam);
    (void)bg;
}

static void end_case(const char *out) {
    cui_iso_end(sc, ui);
    cui_end_frame(ui);
    const cui_draw_data *dd = cui_get_draw_data(ui);
    unsigned char *px = malloc((size_t)dd->width_px * dd->height_px * 4);
    cui_sw_render(sw, dd, px, dd->width_px, dd->height_px, dd->width_px * 4,
                  CUI_SW_RGBA);
    cui_write_png(out, px, dd->width_px, dd->height_px, dd->width_px * 4);
    free(px);
    printf("wrote %s\n", out);
}

/* floor for most cases */
static void floor_grid(void) {
    cui_iso_grid_named(sc,
                       .cx = 0, .cy = 0, .z = 0, .cells = 8, .cell = 44,
                       .fill = CUI_RGB(0x181c23), .line = CUI_RGB(0x242a34));
}

static cui_color G(void) { return CUI_RGB(0x34d18c); }
static cui_color A(void) { return CUI_RGB(0xf5a524); }
static cui_color R(void) { return CUI_RGB(0xe5484d); }
static cui_color B(void) { return CUI_RGB(0x4fa3e0); }
static cui_color N(void) { return CUI_RGB(0x8b9096); }

/* 1. Service topology: a gateway hub with services around it, traffic pulses */
static void case_topology(float t) {
    begin_case("topology");
    floor_grid();
    cui_iso_slab_named(sc,
                       .cx = 0, .cy = 0, .z_top = 0, .w = 70, .d = 70, .height = 12,
                       .top = CUI_RGB(0x3a3f47), .side = CUI_RGB(0x23262b));
    cui_iso_sphere_named(sc,
                         .x = 0, .y = 0, .z = 22, .radius = 11, .color = G());
    cui_iso_ring(sc, 0, 0, 0, 96, 5, t, true, G(), NULL, NULL);
    end_case("/tmp/top10/01_service_topology.png");
}

/* 2. CI/CD pipeline: stages on a conveyor, green passed / amber running */
static void case_pipeline(float t) {
    begin_case("pipeline");
    floor_grid();
    struct { float x; cui_color c; } st[5] = {
        { -120, G() }, { -60, G() }, { 0, A() }, { 60, N() }, { 120, N() } };
    for (int i = 0; i < 5; i++) {
        cui_iso_slab_named(sc,
                           .cx = st[i].x, .cy = 0, .z_top = 0, .w = 40, .d = 40, .height = 10,
                           .top = CUI_RGB(0x3a3f47), .side = CUI_RGB(0x23262b));
        cui_iso_sphere_named(sc,
                             .x = st[i].x, .y = 0, .z = 16, .radius = 7, .color = st[i].c);
        if (i < 4)
            cui_iso_link_named(sc,
                               .x1 = st[i].x + 14, .y1 = 0, .z1 = 6,
                               .x2 = st[i + 1].x - 14, .y2 = 0, .z2 = 6,
                               .thickness = 2.0f, .color = st[i].c,
                               .pulse = (st[i].c.a == A().a) ? t : -1.0f);
    }
    end_case("/tmp/top10/02_ci_pipeline.png");
}

/* 3. Kubernetes cluster: pods grouped on three nodes */
static void case_k8s(float t) {
    begin_case("k8s");
    floor_grid();
    float nx[3] = { -90, 0, 90 };
    for (int n = 0; n < 3; n++) {
        cui_iso_slab_named(sc,
                           .cx = nx[n], .cy = 0, .z_top = 0, .w = 66, .d = 52, .height = 10,
                           .top = CUI_RGB(0x3a3f47), .side = CUI_RGB(0x23262b));
        for (int p = 0; p < 3; p++)
            cui_iso_box_named(sc,
                              .cx = nx[n] - 18 + p * 18, .cy = 0, .z_top = 10,
                              .w = 14, .d = 14, .height = 12, .chamfer = 3,
                              .top = (n == 1 && p == 1) ? A() : B(),
                              .side = CUI_RGB(0x23262b));
    }
    (void)t;
    end_case("/tmp/top10/03_kubernetes.png");
}

/* 4. Data / ETL pipeline: sources funnel into a transform then a warehouse.
 * No hand-computed coordinates: the row layout hands us the three station
 * places, and cui_iso_on stacks each thing on top of its platform. */
static void case_etl(float t) {
    begin_case("etl");
    floor_grid();

    cui_iso_slot station[3];
    cui_iso_layout_row_ex(station, 3, &(cui_iso_layout_row_opts){ .x = 0, .y = 0, .z = 0, .spacing = 110 });

    /* two sources stacked in a small column beside the first station */
    cui_iso_slot src[2];
    cui_iso_layout_row_ex(src, 2, &(cui_iso_layout_row_opts){ .x = station[0].x, .y = 0, .z = 0, .spacing = 80 });
    for (int i = 0; i < 2; i++) {
        cui_iso_slab_named(sc,
                           .cx = src[i].x, .cy = src[i].y, .z_top = 0,
                           .w = 30, .d = 30, .height = 10,
                           .top = CUI_RGB(0x3a3f47), .side = CUI_RGB(0x23262b));
        cui_iso_cylinder_named(sc,
                               .cx = src[i].x, .cy = src[i].y, .z_bottom = 0,
                               .radius = 16, .height = 26, .sides = 20,
                               .top = B(), .side = CUI_RGB(0x1f3a52));
    }

    /* the transform, and the warehouse it feeds */
    cui_iso_box_named(sc,
                      .cx = station[1].x, .cy = station[1].y, .z_top = 0,
                      .w = 46, .d = 46, .height = 30, .chamfer = 5,
                      .top = CUI_RGB(0x5c6169), .side = CUI_RGB(0x2c2f33));
    cui_iso_cylinder_named(sc,
                           .cx = station[2].x, .cy = station[2].y, .z_bottom = 0,
                           .radius = 30, .height = 40, .sides = 24,
                           .top = G(), .side = CUI_RGB(0x1f4a34));

    /* links: the library trims each end to the slot edge — no offsets to work out */
    cui_iso_link_between(sc, cui_iso_on(src[0], 18), cui_iso_on(station[1], 16),
                         20.0f, 2.0f, B(), t);
    cui_iso_link_between(sc, cui_iso_on(src[1], 18), cui_iso_on(station[1], 16),
                         20.0f, 2.0f, B(), t + 0.5f);
    cui_iso_link_between(sc, cui_iso_on(station[1], 16), cui_iso_on(station[2], 20),
                         22.0f, 2.4f, G(), t + 0.25f);
    end_case("/tmp/top10/04_data_etl.png");
}

/* 5. Distributed trace: one request's path, latency on each hop.
 * A row of hops; each hop lifts a little so the path reads as a route. */
static void case_trace(float t) {
    begin_case("trace");
    floor_grid();
    cui_iso_slot hop[4];
    cui_iso_layout_row_ex(hop, 4, &(cui_iso_layout_row_opts){ .x = 0, .y = 0, .z = 0, .spacing = 80 });
    float lift[4] = { 0, 22, 8, 0 };
    for (int i = 0; i < 4; i++) {
        cui_iso_slot at = cui_iso_on(hop[i], lift[i]);
        cui_iso_sphere_named(sc,
                             .x = at.x, .y = at.y, .z = at.z + 16, .radius = 9,
                             .color = i == 2 ? A() : B());
        if (i < 3) {
            cui_iso_slot next = cui_iso_on(hop[i + 1], lift[i + 1]);
            cui_iso_link_labeled_named(sc,
                                       .x1 = at.x, .y1 = at.y, .z1 = at.z + 16,
                                       .x2 = next.x, .y2 = next.y, .z2 = next.z + 16,
                                       .thickness = 2.0f, .color = B(),
                                       .pulse = t + i * 0.3f, .label = "ms",
                                       .value = 12.0f + i * 9, .decimals = 0);
        }
    }
    end_case("/tmp/top10/05_trace.png");
}

/* 6. Network / datacenter: three racks and a core switch */
static void case_datacenter(float t) {
    begin_case("datacenter");
    floor_grid();
    cui_iso_box_named(sc,
                      .cx = 0, .cy = -60, .z_top = 0, .w = 150, .d = 26, .height = 14,
                      .chamfer = 3, .top = CUI_RGB(0x5c6169), .side = CUI_RGB(0x2c2f33));
    for (int i = 0; i < 3; i++) {
        float x = -90 + i * 90;
        cui_iso_disk_stack_named(sc,
                                 .cx = x, .cy = 20, .z_bottom = 0, .radius = 20,
                                 .count = 4, .disk_h = 8, .gap = 4, .sides = 20,
                                 .top = CUI_RGB(0x9fd14f), .side = CUI_RGB(0x3f5a24));
        cui_iso_link_named(sc,
                           .x1 = x, .y1 = 12, .z1 = 18, .x2 = x * 0.3f, .y2 = -48, .z2 = 16,
                           .thickness = 2.0f, .color = G(), .pulse = t + i * 0.33f);
    }
    end_case("/tmp/top10/06_datacenter.png");
}

/* 7. MLOps flow: data -> train -> eval -> deploy with a gate.
 * A row of four stages; the last connector pulses because it is the deploy gate. */
static void case_mlops(float t) {
    begin_case("mlops");
    floor_grid();
    cui_iso_slot s[4];
    cui_iso_layout_row_ex(s, 4, &(cui_iso_layout_row_opts){ .x = 0, .y = 0, .z = 0, .spacing = 80 });
    cui_color col[4] = { B(), B(), A(), G() };
    for (int i = 0; i < 4; i++) {
        cui_iso_slab_named(sc,
                           .cx = s[i].x, .cy = s[i].y, .z_top = 0, .w = 44, .d = 44,
                           .height = 10, .top = CUI_RGB(0x3a3f47), .side = CUI_RGB(0x23262b));
        cui_iso_sphere_named(sc,
                             .x = s[i].x, .y = s[i].y, .z = 16, .radius = 8, .color = col[i]);
        if (i < 3)
            cui_iso_link_between(sc, cui_iso_on(s[i], 6), cui_iso_on(s[i + 1], 6),
                                 16.0f, 2.0f, col[i], (i == 2) ? t : -1.0f);
    }
    end_case("/tmp/top10/07_mlops.png");
}

/* 8. IoT / smart building: sensors on a floor grid reporting readings */
static void case_iot(float t) {
    begin_case("iot");
    floor_grid();
    for (int gx = -1; gx <= 1; gx++)
        for (int gy = -1; gy <= 1; gy++) {
            float x = gx * 70.0f, y = gy * 50.0f;
            cui_iso_cylinder_named(sc,
                                   .cx = x, .cy = y, .z_bottom = 0, .radius = 8,
                                   .height = 14, .sides = 14, .top = CUI_RGB(0x5c6169),
                                   .side = CUI_RGB(0x2c2f33));
            cui_color c = ((gx + gy) % 2 == 0) ? G() : B();
            cui_iso_sphere_named(sc,
                                 .x = x, .y = y, .z = 18, .radius = 6, .color = c);
            cui_iso_link_named(sc,
                               .x1 = x, .y1 = y, .z1 = 20, .x2 = 0, .y2 = 0, .z2 = 26,
                               .thickness = 1.6f, .color = c,
                               .pulse = t + (float)(gx + gy) * 0.2f);
        }
    cui_iso_sphere_named(sc,
                         .x = 0, .y = 0, .z = 34, .radius = 10, .color = A());
    end_case("/tmp/top10/08_iot_building.png");
}

/* 9. Manufacturing line: stations on a conveyor, one down (red).
 * A tight row of stations; station 3 is down, so its colour and the link into
 * the next one both turn red. */
static void case_factory(float t) {
    begin_case("factory");
    floor_grid();
    cui_iso_slot st[5];
    cui_iso_layout_row_ex(st, 5, &(cui_iso_layout_row_opts){ .x = 0, .y = 0, .z = 0, .spacing = 55 });
    for (int i = 0; i < 5; i++) {
        cui_color c = (i == 3) ? R() : (i < 3 ? G() : N());
        cui_iso_box_named(sc,
                          .cx = st[i].x, .cy = st[i].y, .z_top = 0,
                          .w = 34, .d = 30, .height = 18, .chamfer = 4,
                          .top = CUI_RGB(0x3a3f47), .side = CUI_RGB(0x23262b));
        cui_iso_sphere_named(sc,
                             .x = st[i].x, .y = st[i].y, .z = 24, .radius = 6, .color = c);
        if (i < 4)
            cui_iso_link_between(sc, cui_iso_on(st[i], 8), cui_iso_on(st[i + 1], 8),
                                 16.0f, 2.0f, i < 3 ? G() : R(), t);
    }
    end_case("/tmp/top10/09_manufacturing.png");
}

/* 10. Capacity / cost map: resources sized by spend */
static void case_capacity(float t) {
    begin_case("capacity");
    floor_grid();
    cui_iso_box_named(sc,
                      .cx = -90, .cy = -30, .z_top = 0, .w = 60, .d = 50, .height = 70,
                      .chamfer = 6, .top = CUI_RGB(0x5c6169), .side = CUI_RGB(0x2c2f33));
    cui_iso_cylinder_named(sc,
                           .cx = 20, .cy = 20, .z_bottom = 0, .radius = 26, .height = 46,
                           .sides = 22, .top = A(), .side = CUI_RGB(0x5a4518));
    cui_iso_box_named(sc,
                      .cx = 90, .cy = -40, .z_top = 0, .w = 44, .d = 44, .height = 34,
                      .chamfer = 5, .top = CUI_RGB(0x4a4e55), .side = CUI_RGB(0x2c2f33));
    cui_iso_lattice_named(sc,
                          .cx = 90, .cy = 40, .z = 0, .nodes = 8, .sticks = 2,
                          .spread = 22.0f, .ball = 4.0f, .ball_color = B(),
                          .stick_color = B(), .seed = 3u);
    (void)t;
    end_case("/tmp/top10/10_capacity.png");
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "/tmp/top10";
    char cmd[256];
    snprintf(cmd, sizeof cmd, "mkdir -p %s", dir);
    if (system(cmd) != 0) return 1;

    sw = cui_sw_create();
    ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw),
                                     .theme = CUI_THEME_DARK });
    cui_set_viewport(ui, W, H, 1.0f);
    sc = cui_iso_scene_create();
    cui_iso_text_style ts = { .font = CUI_FONT_BODY, .size = 11.0f };
    ts.color = CUI_RGB(0xe8eaee);
    ts.value_color = CUI_RGB(0x8b9096);
    cui_iso_set_text_style(sc, ts);

    float t = 0.3f;
    case_topology(t);
    case_pipeline(t);
    case_k8s(t);
    case_etl(t);
    case_trace(t);
    case_datacenter(t);
    case_mlops(t);
    case_iot(t);
    case_factory(t);
    case_capacity(t);

    cui_iso_scene_destroy(sc);
    cui_destroy(ui);
    cui_sw_destroy(sw);
    return 0;
}