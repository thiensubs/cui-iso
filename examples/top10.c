/* top10.c - renders ten real-world cui-iso scenes to PNGs, one per case.
 * Each is a compact diorama built only from the library's primitives, showing
 * the kind of job the layer is good for. Run from cui-iso/:
 *
 *   ./build/top10 /tmp/top10        (writes /tmp/top10/01.png .. 10.png)
 *
 * then compose them with the PIL script in tools/contact_sheet.py.
 */
#include "cui_iso.h"
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
    cui_draw_rect(ui, (cui_rect){0, 0, W, H}, CUI_CORNERS(0), CUI_RGB(0x14161b));
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
    cui_iso_grid(sc, 0, 0, 0, 8, 44, CUI_RGB(0x181c23), CUI_RGB(0x242a34));
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
    cui_iso_slab(sc, 0, 0, 0, 70, 70, 12, CUI_RGB(0x3a3f47), CUI_RGB(0x23262b));
    cui_iso_sphere(sc, 0, 0, 22, 11, G());
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
        cui_iso_slab(sc, st[i].x, 0, 0, 40, 40, 10,
                     CUI_RGB(0x3a3f47), CUI_RGB(0x23262b));
        cui_iso_sphere(sc, st[i].x, 0, 16, 7, st[i].c);
        if (i < 4)
            cui_iso_link(sc, st[i].x + 14, 0, 6, st[i + 1].x - 14, 0, 6, 2.0f,
                         st[i].c, (st[i].c.a == A().a) ? t : -1.0f);
    }
    end_case("/tmp/top10/02_ci_pipeline.png");
}

/* 3. Kubernetes cluster: pods grouped on three nodes */
static void case_k8s(float t) {
    begin_case("k8s");
    floor_grid();
    float nx[3] = { -90, 0, 90 };
    for (int n = 0; n < 3; n++) {
        cui_iso_slab(sc, nx[n], 0, 0, 66, 52, 10, CUI_RGB(0x3a3f47), CUI_RGB(0x23262b));
        for (int p = 0; p < 3; p++)
            cui_iso_box(sc, nx[n] - 18 + p * 18, 0, 10, 14, 14, 12, 3,
                        (n == 1 && p == 1) ? A() : B(), CUI_RGB(0x23262b));
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
    cui_iso_layout_row(station, 3, 0, 0, 0, 110);

    /* two sources stacked in a small column beside the first station */
    cui_iso_slot src[2];
    cui_iso_layout_row(src, 2, station[0].x, 0, 0, 80);
    for (int i = 0; i < 2; i++) {
        cui_iso_slab(sc, src[i].x, src[i].y, 0, 30, 30, 10,
                     CUI_RGB(0x3a3f47), CUI_RGB(0x23262b));
        cui_iso_cylinder(sc, src[i].x, src[i].y, 0, 16, 26, 20,
                         B(), CUI_RGB(0x1f3a52));
    }

    /* the transform, and the warehouse it feeds */
    cui_iso_box(sc, station[1].x, station[1].y, 0, 46, 46, 30, 5,
                CUI_RGB(0x5c6169), CUI_RGB(0x2c2f33));
    cui_iso_cylinder(sc, station[2].x, station[2].y, 0, 30, 40, 24,
                     G(), CUI_RGB(0x1f4a34));

    /* links: the library trims each end to the slot edge — no offsets to work out */
    cui_iso_link_between(sc, cui_iso_on(src[0], 18), cui_iso_on(station[1], 16),
                         20.0f, 2.0f, B(), t);
    cui_iso_link_between(sc, cui_iso_on(src[1], 18), cui_iso_on(station[1], 16),
                         20.0f, 2.0f, B(), t + 0.5f);
    cui_iso_link_between(sc, cui_iso_on(station[1], 16), cui_iso_on(station[2], 20),
                         22.0f, 2.4f, G(), t + 0.25f);
    end_case("/tmp/top10/04_data_etl.png");
}

/* 5. Distributed trace: one request's path, latency on each hop */
static void case_trace(float t) {
    begin_case("trace");
    floor_grid();
    float hx[4] = { -120, -40, 40, 120 };
    float hz[4] = { 0, 22, 8, 0 };
    for (int i = 0; i < 4; i++) {
        cui_iso_sphere(sc, hx[i], 0, hz[i] + 16, 9, i == 2 ? A() : B());
        if (i < 3)
            cui_iso_link_labeled(sc, hx[i] + 8, 0, hz[i] + 16, hx[i + 1] - 8, 0,
                                 hz[i + 1] + 16, 2.0f, B(), t + i * 0.3f, "ms",
                                 12.0f + i * 9, 0);
    }
    end_case("/tmp/top10/05_trace.png");
}

/* 6. Network / datacenter: three racks and a core switch */
static void case_datacenter(float t) {
    begin_case("datacenter");
    floor_grid();
    cui_iso_box(sc, 0, -60, 0, 150, 26, 14, 3, CUI_RGB(0x5c6169), CUI_RGB(0x2c2f33));
    for (int i = 0; i < 3; i++) {
        float x = -90 + i * 90;
        cui_iso_disk_stack(sc, x, 20, 0, 20, 4, 8, 4, 20,
                           CUI_RGB(0x9fd14f), CUI_RGB(0x3f5a24));
        cui_iso_link(sc, x, 12, 18, x * 0.3f, -48, 16, 2.0f, G(), t + i * 0.33f);
    }
    end_case("/tmp/top10/06_datacenter.png");
}

/* 7. MLOps flow: data -> train -> eval -> deploy with a gate */
static void case_mlops(float t) {
    begin_case("mlops");
    floor_grid();
    struct { float x; const char *n; cui_color c; } s[4] = {
        { -120, "data", B() }, { -40, "train", B() }, { 40, "eval", A() },
        { 120, "deploy", G() } };
    for (int i = 0; i < 4; i++) {
        cui_iso_slab(sc, s[i].x, 0, 0, 44, 44, 10, CUI_RGB(0x3a3f47), CUI_RGB(0x23262b));
        cui_iso_sphere(sc, s[i].x, 0, 16, 8, s[i].c);
        if (i < 3)
            cui_iso_link(sc, s[i].x + 16, 0, 6, s[i + 1].x - 16, 0, 6, 2.0f,
                         s[i].c, (i == 2) ? t : -1.0f);
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
            cui_iso_cylinder(sc, x, y, 0, 8, 14, 14, CUI_RGB(0x5c6169), CUI_RGB(0x2c2f33));
            cui_color c = ((gx + gy) % 2 == 0) ? G() : B();
            cui_iso_sphere(sc, x, y, 18, 6, c);
            cui_iso_link(sc, x, y, 20, 0, 0, 26, 1.6f, c, t + (float)(gx + gy) * 0.2f);
        }
    cui_iso_sphere(sc, 0, 0, 34, 10, A());
    end_case("/tmp/top10/08_iot_building.png");
}

/* 9. Manufacturing line: stations on a conveyor, one down (red) */
static void case_factory(float t) {
    begin_case("factory");
    floor_grid();
    for (int i = 0; i < 5; i++) {
        float x = -110 + i * 55;
        cui_color c = (i == 3) ? R() : (i < 3 ? G() : N());
        cui_iso_box(sc, x, 0, 0, 34, 30, 18, 4, CUI_RGB(0x3a3f47), CUI_RGB(0x23262b));
        cui_iso_sphere(sc, x, 0, 24, 6, c);
        if (i < 4)
            cui_iso_link(sc, x + 16, 0, 8, x + 40, 0, 8, 2.0f, i < 3 ? G() : R(), t);
    }
    end_case("/tmp/top10/09_manufacturing.png");
}

/* 10. Capacity / cost map: resources sized by spend */
static void case_capacity(float t) {
    begin_case("capacity");
    floor_grid();
    cui_iso_box(sc, -90, -30, 0, 60, 50, 70, 6, CUI_RGB(0x5c6169), CUI_RGB(0x2c2f33));
    cui_iso_cylinder(sc, 20, 20, 0, 26, 46, 22, A(), CUI_RGB(0x5a4518));
    cui_iso_box(sc, 90, -40, 0, 44, 44, 34, 5, CUI_RGB(0x4a4e55), CUI_RGB(0x2c2f33));
    cui_iso_lattice(sc, 90, 40, 0, 8, 2, 22.0f, 4.0f, B(), B(), 3u);
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