/* cui_iso.c - implementation of the isometric diorama layer. See cui_iso.h. */
#include "cui_iso.h"
#include "cui_iso_internal.h"

/* Two clip APIs from CUI that stock CUI does not have: the checked push, so a
 * scene can clip without taking the caller's clip, and the current clip, so
 * picking agrees with what was painted. A CUI without them fails here with a
 * sentence, instead of at link time with a bare symbol name. */
#if !defined(CUI_HAS_PUSH_CLIP_CHECKED) || !defined(CUI_HAS_CURRENT_CLIP)
#error "cui-iso needs a CUI with cui_push_clip_checked() and cui_current_clip()"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CUI_PI
#define CUI_PI 3.14159265358979323846f
#endif

#define CUI_ISO_COS30 0.8660254f
#define CUI_ISO_SIN30 0.5f

/* The most divisions per side the ground grid will draw. Capped so the count
 * survives the round trip through the solid's float field and the draw loop
 * stays bounded: a floor with a million divisions is not a scene. */
#define CUI_ISO_MAX_GRID_DIV 1024

/* The most sides a cylinder is drawn with, and the most points one rim hull
 * contributes: the picking code hulls the top and bottom rim together. */
#define CUI_ISO_MAX_CYL_SIDES 64
#define CUI_ISO_MAX_HULL_PTS (2 * CUI_ISO_MAX_CYL_SIDES)

/* Link and arc are drawn as the same three-layer strand: a wide translucent
 * glow, a body, and a bright core. One set of weights for both, so they read as
 * one material instead of two that happen to look alike. */
#define CUI_ISO_STRAND_GLOW_A 0.20f   /* glow alpha  = strand alpha * this */
#define CUI_ISO_STRAND_BODY_A 0.75f   /* body alpha  = strand alpha * this */
#define CUI_ISO_STRAND_CORE_A 235     /* core alpha, 0..255 */
#define CUI_ISO_STRAND_GLOW_W 3.2f    /* layer widths, times strand thickness */
#define CUI_ISO_STRAND_BODY_W 1.6f
#define CUI_ISO_STRAND_CORE_W 0.7f

/* A sphere reads as a ball by the highlight in its upper left. */
#define CUI_ISO_SPHERE_HI_DX 0.30f    /* highlight offset, times radius */
#define CUI_ISO_SPHERE_HI_DY 0.32f
#define CUI_ISO_SPHERE_HI_R 0.42f     /* highlight radius, times radius */
#define CUI_ISO_SPHERE_HI_A 0.55f     /* highlight alpha, times body alpha */

/* Cap and ridge outlines, in pixels. */
#define CUI_ISO_EDGE_W 1.4f
#define CUI_ISO_EDGE_A 0.09f   /* edge alpha, times face alpha */
#define CUI_ISO_FRAME_W 1.6f

/* Fixed capacity: one allocation, no realloc. Far more than a control surface
 * needs; a scene that overflows simply drops the extra solids. */

/* ---- lighting ------------------------------------------------------------ */

cui_iso_light cui_iso_light_default(void) {
    cui_iso_light l = { -0.62f, -0.78f, 0.34f, 1.0f };
    return l;
}

cui_color cui_iso_shade(cui_color base, cui_iso_light light, float nx, float ny) {
    float len = sqrtf(nx * nx + ny * ny);
    if (len > 0.0f) { nx /= len; ny /= len; }
    float facing = 0.5f + 0.5f * (nx * light.x + ny * light.y);
    float k = light.floor + (light.ceil - light.floor) * facing;
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    cui_color c;
    c.r = (uint8_t)(base.r * k + 0.5f);
    c.g = (uint8_t)(base.g * k + 0.5f);
    c.b = (uint8_t)(base.b * k + 0.5f);
    c.a = base.a;
    return c;
}

/* ---- camera -------------------------------------------------------------- */

void cui_iso_basis(const cui_iso_camera *cam, float *cos_az, float *sin_az) {
    *cos_az = cosf(cam->az);
    *sin_az = sinf(cam->az);
}

cui_vec2 cui_iso_project(const cui_iso_camera *cam, float x, float y, float z) {
    float ca, sa;
    cui_iso_basis(cam, &ca, &sa);
    float rx = x * ca - y * sa;
    float ry = x * sa + y * ca;
    cui_vec2 p;
    p.x = cam->cx + (rx - ry) * CUI_ISO_COS30 * cam->scale;
    p.y = cam->cy + (rx + ry) * CUI_ISO_SIN30 * cam->scale - z * cam->scale;
    return p;
}

float cui_iso_depth(const cui_iso_camera *cam, float x, float y) {
    float ca, sa;
    cui_iso_basis(cam, &ca, &sa);
    return (x * ca - y * sa) + (x * sa + y * ca);
}

/* ---- slots --------------------------------------------------------------- */

cui_iso_slot cui_iso_on(cui_iso_slot s, float height) {
    cui_iso_slot r = { s.x, s.y, s.z + height };
    return r;
}

int cui_iso_layout_row(cui_iso_slot *out, int n, float cx, float cy,
                       float z, float spacing) {
    if (!out || n <= 0) return 0;
    for (int i = 0; i < n; i++) {
        /* centred: slot 0 is left-most, so shift back by half the span */
        out[i].x = cx + (float)(i - (n - 1) / 2.0f) * spacing;
        out[i].y = cy;
        out[i].z = z;
    }
    return n;
}

int cui_iso_layout_ring(cui_iso_slot *out, int n, float cx, float cy,
                        float z, float radius) {
    if (!out || n <= 0) return 0;
    for (int i = 0; i < n; i++) {
        float a = (float)i / (float)n * 2.0f * CUI_PI;
        out[i].x = cx + cosf(a) * radius;
        out[i].y = cy + sinf(a) * radius;
        out[i].z = z;
    }
    return n;
}

int cui_iso_layout_grid(cui_iso_slot *out, int cols, int rows,
                        float cx, float cy, float z, float cell) {
    /* A negative dimension would write no slots and still return a positive
     * count, and a product past the cap cannot be counted at all. Both are
     * refused before anything is written, so the caller never reads a slot
     * that was never filled. */
    if (!out || cols <= 0 || rows <= 0) return 0;
    if (cols > CUI_ISO_MAX_GRID_CELLS / rows) return 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            int i = r * cols + c;
            out[i].x = cx + (float)(c - (cols - 1) / 2.0f) * cell;
            out[i].y = cy + (float)(r - (rows - 1) / 2.0f) * cell;
            out[i].z = z;
        }
    return cols * rows;
}

/* Named-argument forms of the three layouts. Thin forwards, so the geometry
 * above is the only geometry: a struct that disagreed with a positional call
 * would be a bug here, not a second implementation. */
int cui_iso_layout_row_ex(cui_iso_slot *out, int n, const cui_iso_layout_row_opts *o) {
    if (!o) return 0;
    return cui_iso_layout_row(out, n, o->x, o->y, o->z, o->spacing);
}

int cui_iso_layout_ring_ex(cui_iso_slot *out, int n, const cui_iso_layout_ring_opts *o) {
    if (!o) return 0;
    return cui_iso_layout_ring(out, n, o->x, o->y, o->z, o->radius);
}

int cui_iso_layout_grid_ex(cui_iso_slot *out, int cols, int rows,
                           const cui_iso_layout_grid_opts *o) {
    if (!o) return 0;
    return cui_iso_layout_grid(out, cols, rows, o->x, o->y, o->z, o->cell);
}

/* ---- solid list ---------------------------------------------------------- */

typedef enum {
    CUI_ISO_SHADOW,
    CUI_ISO_BOX,
    CUI_ISO_GLASS,
    CUI_ISO_CYLINDER,   /* vertical cylinder: pillars, tanks, columns */
    CUI_ISO_SPHERE,     /* depth-sorted ball: molecules, node markers, particles */
    CUI_ISO_LINK,       /* a connection between two world points: pipes, conduits */
    CUI_ISO_ARC,        /* a curved link that bulges above the straight path */
    CUI_ISO_TEXT,       /* a caption anchored at a world point */
    CUI_ISO_WEDGE,      /* a triangular prism: pitched roof / ramp */
} cui_iso_kind;

typedef struct {
    cui_iso_kind kind;
    float cx, cy, z;         /* anchor: box -> top centre, glass -> bottom, shadow -> plane */
    float w, d, height;
    float chamfer;
    /* second endpoint, used by CUI_ISO_LINK */
    float x2, y2, z2;
    float radius;            /* cylinder/sphere radius, link thickness */
    float pulse;             /* link: 0..1 position of a travelling dot, <0 = none */
    float bias;              /* tie-break added to the painter's key */
    const char *label;       /* link/text: optional text drawn at the anchor */
    float label_value;       /* link: optional number shown next to the label */
    int label_decimals;      /* link: digits after the point for label_value */
    cui_color label_color;   /* link/text: label text colour */
    cui_color top;           /* box top / glass tint / shadow / sphere colour */
    cui_color side;          /* box side base / link colour */
} cui_iso_solid;

/* How labels are drawn (font, size, colour). Public type; see cui_iso.h. */
struct cui_iso_scene {
    cui_iso_camera cam;
    cui_iso_light light;
    cui_iso_text_style text;
    cui_iso_solid solids[CUI_ISO_MAX_SOLIDS];
    int count;
    bool begun;
    bool reduced_motion;     /* freeze pulses at rest; CUI reduced-motion path */
    bool has_bounds;         /* set by cui_iso_set_bounds; else draw anywhere */
    cui_rect bounds;
    /* The clip the last successful paint was drawn under: the scene's bounds
     * intersected with the host's own clip. Picking has to use this and not
     * the pending bounds: the pending bounds are a request, and what a click
     * can hit is what was painted. */
    bool has_painted_clip;
    cui_rect painted_clip;
};

cui_iso_scene *cui_iso_scene_create(void) {
    cui_iso_scene *s = (cui_iso_scene *)calloc(1, sizeof *s);
    if (s) {
        s->light = cui_iso_light_default();
        s->text.size = 12.0f;
        s->text.color = CUI_COLOR_FOREGROUND;   /* token: reads on either theme */
        s->text.value_color = CUI_COLOR_TEXT_SECONDARY;
    }
    return s;
}

void cui_iso_set_text_style(cui_iso_scene *s, cui_iso_text_style style) {
    if (!s) return;
    s->text = style;
    if (s->text.size <= 0.0f) s->text.size = 12.0f;
    /* The colours are kept as given. A theme token has alpha 0, so "alpha 0
     * means unset" would throw the caller's token away; the draw pass resolves
     * the token first and only then treats a still-transparent colour as
     * unset. */
}

void cui_iso_set_light(cui_iso_scene *s, cui_iso_light light) {
    if (s) s->light = light;
}

void cui_iso_set_bounds(cui_iso_scene *s, cui_rect r) {
    if (!s) return;
    /* A zero or negative size clears the clip, so the same scene can move from
     * a clipped canvas to drawing anywhere without replacing the scene. */
    if (r.w <= 0.0f || r.h <= 0.0f) {
        s->has_bounds = false;
        return;
    }
    s->bounds = r;
    s->has_bounds = true;
}

void cui_iso_scene_destroy(cui_iso_scene *s) { free(s); }

int cui_iso_scene_count(const cui_iso_scene *s) { return s ? s->count : 0; }

void cui_iso_scene_reset(cui_iso_scene *s) {
    if (!s) return;
    s->count = 0;
    s->begun = false;
}

void cui_iso_begin(cui_iso_scene *s, const cui_iso_camera *cam) {
    if (!s) return;
    if (!cam) {
        /* A NULL camera invalidates the batch. Leaving the previous batch
         * alive would draw the last frame's solids against the last frame's
         * camera at the next cui_iso_end. */
        s->count = 0;
        s->begun = false;
        return;
    }
    s->cam = *cam;
    s->count = 0;
    s->begun = true;
}

/* Add a solid, or NULL when the scene is NULL, not begun, or full. The scene
 * is checked before it is touched, so a mis-wired caller draws nothing
 * instead of reading through a null pointer. */
static cui_iso_solid *cui_iso__push(cui_iso_scene *s, cui_iso_kind kind) {
    if (!s || !s->begun) return NULL;
    if (s->count >= CUI_ISO_MAX_SOLIDS) return NULL;
    cui_iso_solid *o = &s->solids[s->count++];
    memset(o, 0, sizeof *o);
    o->kind = kind;
    return o;
}

void cui_iso_box(cui_iso_scene *s, float cx, float cy, float z_top,
                 float w, float d, float height, float chamfer,
                 cui_color top, cui_color side) {
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_BOX);
    if (!o) return;
    o->cx = cx; o->cy = cy; o->z = z_top;
    o->w = w; o->d = d; o->height = height;
    float c = chamfer;
    if (c > w * 0.5f) c = w * 0.5f;
    if (c > d * 0.5f) c = d * 0.5f;
    if (c < 0.0f) c = 0.0f;
    o->chamfer = c;
    o->top = top;
    o->side = side;
}

void cui_iso_slab(cui_iso_scene *s, float cx, float cy, float z_top,
                  float w, float d, float height, cui_color top, cui_color side) {
    float ch = w < d ? w : d;
    ch *= 0.10f;                 /* gentle chamfer, matches the platform look */
    if (ch > height) ch = height;
    cui_iso_box(s, cx, cy, z_top, w, d, height, ch, top, side);
}

void cui_iso_glass(cui_iso_scene *s, float cx, float cy, float z_bottom,
                   float w, float d, float height, cui_color tint) {
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_GLASS);
    if (!o) return;
    o->cx = cx; o->cy = cy; o->z = z_bottom;
    o->w = w; o->d = d; o->height = height;
    o->top = tint;
}

void cui_iso_shadow(cui_iso_scene *s, float cx, float cy, float z,
                    float rx, float ry, cui_color color) {
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_SHADOW);
    if (!o) return;
    o->cx = cx; o->cy = cy; o->z = z;
    o->w = rx; o->d = ry;        /* reuse w/d as screen radii */
    o->top = color;
}

/* A tiled ground plane under the whole scene — the floor every NVIDIA diorama
 * sits on. Drawn first (like a shadow), before all solids. (cx, cy) is the
 * centre at height z; `cells` is the number of divisions along each side and
 * `cell` the world size of one tile, so the plane spans cells*cell wide.
 * `line` is the grid-line colour; a zero-alpha `fill` leaves the floor
 * transparent so your own background shows through. */
void cui_iso_grid(cui_iso_scene *s, float cx, float cy, float z,
                  int cells, float cell, cui_color fill, cui_color line) {
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_SHADOW);
    if (!o) return;
    /* The cell count rides in a float field and the draw pass reads it back
     * as an int, so it is clamped before it is stored: a count past the cap
     * does not survive the round trip, and the cast back would be undefined. */
    if (cells < 0) cells = 0;
    if (cells > CUI_ISO_MAX_GRID_DIV) cells = CUI_ISO_MAX_GRID_DIV;
    o->cx = cx; o->cy = cy; o->z = z;
    o->w = (float)cells;         /* reuse w as cell count */
    o->d = cell;                 /* reuse d as cell size */
    o->height = 0.0f;
    o->chamfer = 1.0f;           /* flag: this shadow is a grid, not an ellipse */
    o->top = fill;
    o->side = line;
}

void cui_iso_cylinder(cui_iso_scene *s, float cx, float cy, float z_bottom,
                      float radius, float height, int sides,
                      cui_color top, cui_color side) {
    /* depth key at the footprint centre; the side is shaded as a cylinder by
     * splitting it into `sides` vertical strips, each lit by its own normal. */
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_CYLINDER);
    if (!o) return;
    /* The side count rides in a float field and the draw pass reads it back as
     * an int, so it is clamped here rather than at draw time: an unbounded
     * count does not survive the round trip, and the cast back is undefined.
     * The clamp is the same 8..64 the draw pass applies. */
    if (sides < 8) sides = 8;
    if (sides > CUI_ISO_MAX_CYL_SIDES) sides = CUI_ISO_MAX_CYL_SIDES;
    o->cx = cx; o->cy = cy; o->z = z_bottom;
    o->radius = radius; o->height = height;
    o->chamfer = (float)sides;   /* reuse chamfer as side count */
    o->top = top; o->side = side;
}

void cui_iso_sphere(cui_iso_scene *s, float x, float y, float z,
                    float radius, cui_color color) {
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_SPHERE);
    if (!o) return;
    o->cx = x; o->cy = y; o->z = z;
    o->radius = radius;
    o->top = color;
}

void cui_iso_link(cui_iso_scene *s, float x1, float y1, float z1,
                  float x2, float y2, float z2, float thickness,
                  cui_color color, float pulse) {
    /* sort by the midpoint so a link sits between the solids it joins */
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_LINK);
    if (!o) return;
    o->cx = x1; o->cy = y1; o->z = z1;
    o->x2 = x2; o->y2 = y2; o->z2 = z2;
    o->radius = thickness;
    o->side = color;
    o->pulse = pulse;
}

void cui_iso_link_between(cui_iso_scene *s, cui_iso_slot a, cui_iso_slot b,
                          float gap, float thickness, cui_color color,
                          float pulse) {
    /* walk from each slot toward the other by `gap`, so the link meets the edge
     * of whatever sits on the slot instead of its centre. */
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx * dx + dy * dy);
    float ux = len > 0.0f ? dx / len : 0.0f;
    float uy = len > 0.0f ? dy / len : 0.0f;
    cui_iso_link(s, a.x + ux * gap, a.y + uy * gap, a.z,
                 b.x - ux * gap, b.y - uy * gap, b.z, thickness, color, pulse);
}

/* A link that carries a label and a number — the "LLM --3.2k tok/s--> Optimize"
 * look from the NVIDIA pipeline art. The label sits at the midpoint, lifted
 * above the line; the number renders in value_color next to it. Both are drawn
 * after the line, so they float on top and stay readable. */
void cui_iso_link_labeled(cui_iso_scene *s, float x1, float y1, float z1,
                          float x2, float y2, float z2, float thickness,
                          cui_color color, float pulse,
                          const char *label, float value, int decimals) {
    int before = s ? s->count : 0;
    cui_iso_link(s, x1, y1, z1, x2, y2, z2, thickness, color, pulse);
    /* the label rides on the solid this call pushed, not on whatever was last */
    if (s && s->count == before + 1) {
        cui_iso_solid *o = &s->solids[s->count - 1];
        if (o->kind == CUI_ISO_LINK) {
            o->label = label;
            o->label_value = value;
            o->label_decimals = decimals;
            o->label_color = s->text.color;
        }
    }
}

/* A curved conduit that bulges above the straight path between two points —
 * the ring conduits and curved flow arrows in the reference art. `bulge` is
 * how far (world units) the midpoint lifts toward the camera and up. */
void cui_iso_arc(cui_iso_scene *s, float x1, float y1, float z1,
                 float x2, float y2, float z2, float bulge, float thickness,
                 cui_color color, float pulse) {
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_ARC);
    if (!o) return;
    o->cx = x1; o->cy = y1; o->z = z1;
    o->x2 = x2; o->y2 = y2; o->z2 = z2;
    o->chamfer = bulge;
    o->radius = thickness;
    o->side = color;
    o->pulse = pulse;
}

/* A ball-and-stick graph floating at a world point — the recurring neural-net
 * motif in the NVIDIA art, in ONE call instead of twenty sphere+link calls.
 * Places `nodes` balls deterministically in a small cloud (seeded, so it does
 * not jitter frame to frame), then joins each ball to its `sticks` nearest
 * neighbours by distance.
 * `spread` is the cloud radius in world units, `ball` the ball radius. */
void cui_iso_lattice(cui_iso_scene *s, float cx, float cy, float z,
                     int nodes, int sticks, float spread, float ball,
                     cui_color ball_color, cui_color stick_color, uint32_t seed) {
    if (!s) return;
    if (nodes < 2) nodes = 2;
    if (nodes > 24) nodes = 24;
    /* A node has at most nodes-1 neighbours, so more sticks than that asks
     * for a neighbour that does not exist. */
    if (sticks < 1) sticks = 1;
    if (sticks > nodes - 1) sticks = nodes - 1;
    float px[24], py[24], pz[24];
    uint32_t st = seed * 2654435761u + 12345u;
    for (int i = 0; i < nodes; i++) {
        /* deterministic LCG, same trick mcpd-ui uses for its molecules */
        st = st * 1664525u + 1013904223u;
        px[i] = (float)((int)((st >> 8) % 1000) - 500) / 500.0f * spread;
        st = st * 1664525u + 1013904223u;
        py[i] = (float)((int)((st >> 8) % 1000) - 500) / 500.0f * spread;
        st = st * 1664525u + 1013904223u;
        pz[i] = (float)((int)((st >> 8) % 1000)) / 1000.0f * spread * 0.7f;
    }
    for (int i = 0; i < nodes; i++)
        cui_iso_sphere(s, cx + px[i], cy + py[i], z + pz[i], ball, ball_color);
    /* Nearest neighbours by distance, not by array order: the old loop joined
     * node i to the nodes that came after it, so node 0 stood alone and a
     * two-node graph drew no edge at all. Each pair is drawn once -- the
     * nearest-neighbour relation is symmetric, so both ends would otherwise
     * add the same stick twice. */
    bool linked[24][24];
    memset(linked, 0, sizeof linked);
    for (int i = 0; i < nodes; i++) {
        bool taken[24];
        memset(taken, 0, sizeof taken);
        for (int k = 0; k < sticks; k++) {
            int best = -1;
            float bestd = 0.0f;
            for (int j = 0; j < nodes; j++) {
                if (j == i || taken[j]) continue;
                float dx = px[j] - px[i], dy = py[j] - py[i], dz = pz[j] - pz[i];
                float d = dx * dx + dy * dy + dz * dz;
                if (best < 0 || d < bestd) { best = j; bestd = d; }
            }
            if (best < 0) break;
            taken[best] = true;
            int a = i < best ? i : best;
            int b = i < best ? best : i;
            if (linked[a][b]) continue;
            linked[a][b] = true;
            cui_iso_link(s, cx + px[i], cy + py[i], z + pz[i],
                         cx + px[best], cy + py[best], z + pz[best],
                         ball * 0.35f, stick_color, -1.0f);
        }
    }
}

/* A stack of `count` flat disks (the coin / database drum in the art). Each
 * disk is a short wide cylinder; `gap` is the visible spacing between them. */
void cui_iso_disk_stack(cui_iso_scene *s, float cx, float cy, float z_bottom,
                        float radius, int count, float disk_h, float gap,
                        int sides, cui_color top, cui_color side) {
    /* Nothing to draw and nowhere to draw it: a null scene, a scene that is not
     * in a batch, a non-positive count or a full batch all mean the loop below
     * must not run. The count is also capped at the room left in the batch, so
     * a stack of a million disks stops after the disks that fit instead of
     * making a million refused insertions. */
    if (!s || !s->begun || count <= 0) return;
    int room = CUI_ISO_MAX_SOLIDS - s->count;
    if (room <= 0) return;
    if (count > room) count = room;
    for (int i = 0; i < count; i++) {
        float zb = z_bottom + (float)i * (disk_h + gap);
        cui_iso_cylinder(s, cx, cy, zb, radius, disk_h, sides, top, side);
    }
}

/* A caption anchored at a world point. Captions are drawn in a final overlay
 * pass after every solid, so a caption is never painted over. The string is
 * kept by pointer and read when the frame is drawn: it must stay valid until
 * cui_iso_end returns. Pass a colour with alpha 0 to use the scene's text
 * colour. */
void cui_iso_text(cui_iso_scene *s, float x, float y, float z,
                  const char *text, cui_color color) {
    /* A null caption would take a slot in the batch and draw nothing, so it
     * is refused here rather than counted as a solid. */
    if (!text) return;
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_TEXT);
    if (!o) return;
    o->cx = x; o->cy = y; o->z = z;
    o->label = text;
    /* Kept as given, token or not: the draw pass resolves it and decides
     * whether an unset colour falls back to the scene's text colour. */
    o->label_color = color;
}

/* A triangular prism: a box whose top is a single ridge line running along the
 * x axis, i.e. a pitched roof or a ramp. (cx, cy) is the footprint centre, the
 * footprint is w (along the ridge) by d, and the ridge rises `height` above
 * z_base. The two roof faces are shaded by the light like box sides. */
void cui_iso_wedge(cui_iso_scene *s, float cx, float cy, float z_base,
                   float w, float d, float height, cui_color roof, cui_color side) {
    cui_iso_solid *o = cui_iso__push(s, CUI_ISO_WEDGE);
    if (!o) return;
    o->cx = cx; o->cy = cy; o->z = z_base;
    o->w = w; o->d = d; o->height = height;
    o->top = roof;
    o->side = side;
}

void cui_iso_set_reduced_motion(cui_iso_scene *s, bool on) {
    if (!s) return;
    s->reduced_motion = on;
}

/* Place `count` solids evenly around a circle of `radius` at height z, the
 * hub-and-spoke layout in the control-surface art. For each ring position this
 * calls `place` with the position index, its world x/y and a 0..1 angle
 * fraction; you draw whatever solid belongs there. Returns the x/y of slot 0 so
 * a hub at the centre can link out to it. When `conduit` is non-zero an arc is
 * also drawn from the centre out to each position, pulsing in sequence. */
void cui_iso_ring(cui_iso_scene *s, float cx, float cy, float z, float radius,
                  int count, float t, bool conduit, cui_color conduit_color,
                  void (*place)(cui_iso_scene *s, int index, float x, float y,
                                float angle_frac, void *user),
                  void *user) {
    for (int i = 0; i < count; i++) {
        float a = (float)i / (float)count * 2.0f * CUI_PI;
        float x = cx + cosf(a) * radius;
        float y = cy + sinf(a) * radius;
        if (conduit) {
            float p = (t - (float)i / (float)count);
            p -= floorf(p);                    /* stagger each spoke */
            cui_iso_arc(s, cx, cy, z + 20.0f, x, y, z, radius * 0.35f, 2.2f,
                        conduit_color, p);
        }
        if (place) place(s, i, x, y, (float)i / (float)count, user);
    }
}
static void cui_iso__draw_box(cui_ctx *ui, const cui_iso_camera *cam, const cui_iso_light *light,
                     const cui_iso_solid *o) {
    float x0 = o->cx - o->w * 0.5f, x1 = o->cx + o->w * 0.5f;
    float y0 = o->cy - o->d * 0.5f, y1 = o->cy + o->d * 0.5f;
    float c = o->chamfer;
    float zt = o->z, zb = o->z - o->height;

    /* A theme token is a sentinel, not a colour: its bytes are an id, so
     * shading them as bytes turns the id into a literal RGB and leaves the
     * alpha at 0 -- the face then vanishes, or leaks the id as a colour.
     * Resolve first, then shade. */
    cui_color top = cui_color_of(ui, o->top);
    cui_color side = cui_color_of(ui, o->side);

    float wx[8] = { x0 + c, x1 - c, x1,     x1,     x1 - c, x0 + c, x0,     x0     };
    float wy[8] = { y0,     y0,     y0 + c, y1 - c, y1,     y1,     y1 - c, y0 + c };

    cui_vec2 t[8], b[8];
    for (int i = 0; i < 8; i++) {
        t[i] = cui_iso_project(cam, wx[i], wy[i], zt);
        b[i] = cui_iso_project(cam, wx[i], wy[i], zb);
    }

    /* draw the four-ish side faces back to front by their mid-edge depth */
    int order[8];
    float depth[8];
    for (int i = 0; i < 8; i++) {
        order[i] = i;
        depth[i] = cui_iso_depth(cam, (wx[i] + wx[(i + 1) % 8]) * 0.5f,
                                 (wy[i] + wy[(i + 1) % 8]) * 0.5f);
    }
    for (int i = 1; i < 8; i++) {
        int cur = order[i];
        float cd = depth[i];
        int j = i - 1;
        while (j >= 0 && depth[j] > cd) { order[j + 1] = order[j]; depth[j + 1] = depth[j]; j--; }
        order[j + 1] = cur;
        depth[j + 1] = cd;
    }

    for (int k = 0; k < 8; k++) {
        int i = order[k];
        int j = (i + 1) % 8;
        /* skip the degenerate back edges created by the chamfer (zero-length
         * mid-edges would be the back corner); drawing them is harmless but the
         * normal is ill-defined, so shade by the real edge normal instead. */
        float mx = (wx[i] + wx[j]) * 0.5f - o->cx;
        float my = (wy[i] + wy[j]) * 0.5f - o->cy;
        cui_color face = cui_iso_shade(side, *light, mx, my);
        cui_vec2 q[4] = { t[i], t[j], b[j], b[i] };
        cui_draw_polygon(ui, q, 4, face);
    }

    cui_draw_polygon(ui, t, 8, top);

    cui_vec2 ring[9];
    for (int i = 0; i < 8; i++) ring[i] = t[i];
    ring[8] = t[0];
    cui_color edge = top;
    edge.a = (uint8_t)(edge.a * CUI_ISO_EDGE_A + 0.5f);
    cui_draw_polyline(ui, ring, 9, CUI_ISO_EDGE_W, edge, false);
}

/* A translucent glass case with a neon top frame. */
static void cui_iso__draw_glass(cui_ctx *ui, const cui_iso_camera *cam, const cui_iso_solid *o) {
    float x0 = o->cx - o->w * 0.5f, x1 = o->cx + o->w * 0.5f;
    float y0 = o->cy - o->d * 0.5f, y1 = o->cy + o->d * 0.5f;
    float zb = o->z, zt = o->z + o->height;
    cui_vec2 p[8] = {
        cui_iso_project(cam, x0, y0, zt), cui_iso_project(cam, x1, y0, zt),
        cui_iso_project(cam, x1, y1, zt), cui_iso_project(cam, x0, y1, zt),
        cui_iso_project(cam, x0, y0, zb), cui_iso_project(cam, x1, y0, zb),
        cui_iso_project(cam, x1, y1, zb), cui_iso_project(cam, x0, y1, zb),
    };

    /* Resolve the tint before it is faded: a token's bytes are an id, so
     * fading them as bytes would leave the frame transparent and leak the id
     * into the colour. */
    cui_color tint = cui_color_of(ui, o->top);
    cui_draw_polygon(ui, (cui_vec2[]){ p[0], p[1], p[5], p[4] }, 4, tint);
    cui_draw_polygon(ui, (cui_vec2[]){ p[1], p[2], p[6], p[5] }, 4, tint);
    cui_draw_polygon(ui, (cui_vec2[]){ p[2], p[3], p[7], p[6] }, 4, tint);
    cui_draw_polygon(ui, (cui_vec2[]){ p[3], p[0], p[4], p[7] }, 4, tint);
    cui_draw_polygon(ui, (cui_vec2[]){ p[0], p[1], p[2], p[3] }, 4, tint);

    cui_vec2 top[5] = { p[0], p[1], p[2], p[3], p[0] };
    cui_color frame = tint;
    frame.a = 215;
    cui_draw_polyline(ui, top, 5, CUI_ISO_FRAME_W, frame, false);
    for (int i = 0; i < 4; i++) {
        cui_vec2 e[2] = { p[i], p[i + 4] };
        cui_color ef = tint;
        ef.a = 130;
        cui_draw_polyline(ui, e, 2, CUI_ISO_EDGE_W, ef, false);
    }
}

static void cui_iso__draw_shadow(cui_ctx *ui, const cui_iso_camera *cam, const cui_iso_solid *o) {
    cui_vec2 c = cui_iso_project(cam, o->cx, o->cy, o->z);

    /* Resolve first: a token is a sentinel with alpha 0, and "alpha 0 leaves
     * the floor transparent" is about a literal colour. */
    cui_color top = cui_color_of(ui, o->top);
    cui_color side = cui_color_of(ui, o->side);

    /* chamfer==1 marks a ground grid rather than an elliptical blob */
    if (o->chamfer == 1.0f) {
        int cells = (int)o->w;
        float cell = o->d;
        if (cells > 0 && cell > 0.0f) {
            float half = (float)cells * cell * 0.5f;
            if (top.a > 0) {
                /* floor slab as one filled diamond (the plane's footprint) */
                cui_vec2 q[4] = {
                    cui_iso_project(cam, o->cx - half, o->cy - half, o->z),
                    cui_iso_project(cam, o->cx + half, o->cy - half, o->z),
                    cui_iso_project(cam, o->cx + half, o->cy + half, o->z),
                    cui_iso_project(cam, o->cx - half, o->cy + half, o->z),
                };
                cui_draw_polygon(ui, q, 4, top);
            }
            for (int i = 0; i <= cells; i++) {
                float p = -half + (float)i * cell;
                cui_vec2 a = cui_iso_project(cam, o->cx + p, o->cy - half, o->z);
                cui_vec2 b = cui_iso_project(cam, o->cx + p, o->cy + half, o->z);
                cui_draw_line(ui, a, b, 1.0f, side);
                cui_vec2 c2 = cui_iso_project(cam, o->cx - half, o->cy + p, o->z);
                cui_vec2 d2 = cui_iso_project(cam, o->cx + half, o->cy + p, o->z);
                cui_draw_line(ui, c2, d2, 1.0f, side);
            }
        }
        return;
    }

    cui_vec2 pts[24];
    for (int i = 0; i < 24; i++) {
        float a = (float)i / 24.0f * 2.0f * CUI_PI;
        pts[i].x = c.x + cosf(a) * o->w;
        pts[i].y = c.y + sinf(a) * o->d;
    }
    cui_draw_polygon(ui, pts, 24, top);
}

/* A vertical cylinder: a lit side (vertical strips, each its own normal) plus a
 * flat elliptical top. Drawn as a single convex-ish silhouette, so it depth
 * sorts as one solid. */
static void cui_iso__draw_cylinder(cui_ctx *ui, const cui_iso_camera *cam,
                          const cui_iso_light *light, const cui_iso_solid *o) {
    int sides = (int)o->chamfer;
    if (sides < 8) sides = 8;
    if (sides > CUI_ISO_MAX_CYL_SIDES) sides = CUI_ISO_MAX_CYL_SIDES;
    float zb = o->z, zt = o->z + o->height;

    /* Resolve before shading: a token is a sentinel, and shading its bytes
     * turns the id into a colour and leaves the side transparent. */
    cui_color top = cui_color_of(ui, o->top);
    cui_color side = cui_color_of(ui, o->side);

    cui_vec2 tb[65], bb[65];
    for (int i = 0; i <= sides; i++) {
        float a = (float)i / (float)sides * 2.0f * CUI_PI;
        float px = o->cx + cosf(a) * o->radius;
        float py = o->cy + sinf(a) * o->radius;
        tb[i] = cui_iso_project(cam, px, py, zt);
        bb[i] = cui_iso_project(cam, px, py, zb);
    }

    /* Side strips, painter's order: draw back to front so each front strip
     * overwrites the ones behind it. Drawing every strip (not just front-facing
     * ones) is what closes the silhouette — culling the back half by normal
     * leaves anti-aliasing seams at the outline and an open-looking bottom. */
    int order[64];
    float key[64];
    for (int i = 0; i < sides; i++) {
        order[i] = i;
        float am = ((float)i + 0.5f) / (float)sides * 2.0f * CUI_PI;
        /* The key is camera-relative depth, not a world angle: the projection
         * rotates with cam->az, so a fixed angle inverts once the camera passes
         * az = pi/4 and the rear strips would paint over the visible barrel.
         * The rim centre is shared by every strip, so the depth offset is a
         * constant and only the order changes, and only off az = 0. */
        key[i] = cui_iso_depth(cam, cosf(am), sinf(am));  /* larger = closer to camera */
    }
    for (int i = 1; i < sides; i++) {
        int cur = order[i];
        float ck = key[i];
        int j = i - 1;
        while (j >= 0 && key[order[j]] > ck) { order[j + 1] = order[j]; j--; }
        order[j + 1] = cur;
    }
    for (int k = 0; k < sides; k++) {
        int i = order[k];
        float am = ((float)i + 0.5f) / (float)sides * 2.0f * CUI_PI;
        cui_color c = cui_iso_shade(side, *light, cosf(am), sinf(am));
        cui_vec2 q[4] = { tb[i], tb[i + 1], bb[i + 1], bb[i] };
        cui_draw_polygon(ui, q, 4, c);
    }

    /* Top cap: the rim polygon itself. The centre is not part of the polygon:
     * a fan with the centre as apex leaves the radial edges inside the cap,
     * and every one of them is a seam where two triangles meet. */
    cui_vec2 cap[CUI_ISO_MAX_CYL_SIDES];
    for (int i = 0; i < sides; i++) cap[i] = tb[i];
    cui_draw_polygon(ui, cap, sides, top);
}

/* A depth-sorted ball with a simple top-left highlight, so it reads as a sphere
 * rather than a flat disc. */
static void cui_iso__draw_sphere(cui_ctx *ui, const cui_iso_camera *cam, const cui_iso_solid *o) {
    cui_vec2 c = cui_iso_project(cam, o->cx, o->cy, o->z);
    float r = o->radius * cam->scale;
    /* Resolve before the highlight fades it: a token's alpha is 0, so fading
     * the token's bytes would draw no highlight at all. */
    cui_color top = cui_color_of(ui, o->top);
    cui_draw_circle(ui, c, r, top);
    cui_vec2 hi = { c.x - r * CUI_ISO_SPHERE_HI_DX, c.y - r * CUI_ISO_SPHERE_HI_DY };
    cui_color h = top;
    h.a = (uint8_t)(h.a * CUI_ISO_SPHERE_HI_A + 0.5f);
    cui_draw_circle(ui, hi, r * CUI_ISO_SPHERE_HI_R, h);
}

/* The colour a caption is drawn in.
 *
 * A theme token is resolved first: its alpha is 0, and "alpha 0 means use the
 * scene's colour" is about a literal colour, so resolving first is what keeps
 * a caller's token from being read as unset. A colour that is still
 * transparent after the resolution is unset: it falls back to the scene's text
 * colour, and then to the foreground. */
static cui_color iso__label_color(cui_ctx *ui, cui_color c,
                                  const cui_iso_text_style *style) {
    cui_color col = cui_color_of(ui, c);
    if (col.a == 0) col = cui_color_of(ui, style->color);
    if (col.a == 0) col = cui_color_of(ui, CUI_COLOR_FOREGROUND);
    return col;
}

/* A caption anchored at a world point, centred on it. Drawn in the opaque pass
 * so it depth-sorts against the solids around it. */
static void cui_iso__draw_text(cui_ctx *ui, const cui_iso_camera *cam,
                      const cui_iso_text_style *text, const cui_iso_solid *o) {
    if (!o->label) return;
    cui_vec2 c = cui_iso_project(cam, o->cx, o->cy, o->z);
    cui_text_metrics m = cui_measure_text(ui, text->font, text->size,
                                          o->label, (int)strlen(o->label));
    cui_vec2 at = { c.x - m.width * 0.5f, c.y - m.ascent * 0.5f };
    cui_draw_text(ui, text->font, text->size, at,
                  iso__label_color(ui, o->label_color, text),
                  o->label, (int)strlen(o->label));
}

/* A triangular prism — pitched roof or ramp. The ridge runs along the x axis.
 * Two sloped roof faces (shaded by the light) plus two triangular gable ends.
 *
 * Only the faces that point at the camera are drawn. The projection maps
 * (1,1,1) in the rotated frame onto the view axis, so the viewer sits along
 * v = (ca + sa, ca - sa, 1) in world space, and a face is visible when its
 * outward normal has a positive dot with v. Drawing the far side as well put
 * the back gable and the back roof over the near ones. */
static void cui_iso__draw_wedge(cui_ctx *ui, const cui_iso_camera *cam,
                       const cui_iso_light *light, const cui_iso_solid *o) {
    float x0 = o->cx - o->w * 0.5f, x1 = o->cx + o->w * 0.5f;
    float y0 = o->cy - o->d * 0.5f, y1 = o->cy + o->d * 0.5f;
    float zb = o->z, zr = o->z + o->height;   /* base and ridge heights */

    /* the four base corners and the two ridge points */
    cui_vec2 b00 = cui_iso_project(cam, x0, y0, zb);
    cui_vec2 b10 = cui_iso_project(cam, x1, y0, zb);
    cui_vec2 b01 = cui_iso_project(cam, x0, y1, zb);
    cui_vec2 b11 = cui_iso_project(cam, x1, y1, zb);
    cui_vec2 r0 = cui_iso_project(cam, x0, o->cy, zr);
    cui_vec2 r1 = cui_iso_project(cam, x1, o->cy, zr);

    /* Resolve before shading: a token is a sentinel, and shading its bytes
     * turns the id into a colour and leaves the face transparent. */
    cui_color top = cui_color_of(ui, o->top);
    cui_color side = cui_color_of(ui, o->side);

    float ca, sa;
    cui_iso_basis(cam, &ca, &sa);
    float vx = ca + sa, vy = ca - sa;
    float half_d = (y1 - y0) * 0.5f;   /* the ridge sits mid-depth */

    /* roof face toward -y: normal (0, -height, half_d) */
    if (-o->height * vy + half_d > 0.0f) {
        cui_color roof_neg = cui_iso_shade(top, *light, 0.0f, -0.85f);
        cui_draw_polygon(ui, (cui_vec2[]){ b00, b10, r1, r0 }, 4, roof_neg);
    }
    /* roof face toward +y: normal (0, +height, half_d) */
    if (o->height * vy + half_d > 0.0f) {
        cui_color roof_pos = cui_iso_shade(top, *light, 0.0f, 0.85f);
        cui_draw_polygon(ui, (cui_vec2[]){ b01, b11, r1, r0 }, 4, roof_pos);
    }

    /* gable ends (triangles). Shade the -x and +x faces by their normals. */
    if (-vx > 0.0f) {
        cui_color gable_neg = cui_iso_shade(side, *light, -1.0f, 0.0f);
        cui_draw_polygon(ui, (cui_vec2[]){ b00, b01, r0 }, 3, gable_neg);
    }
    if (vx > 0.0f) {
        cui_color gable_pos = cui_iso_shade(side, *light, 1.0f, 0.0f);
        cui_draw_polygon(ui, (cui_vec2[]){ b10, b11, r1 }, 3, gable_pos);
    }

    /* ridge highlight */
    cui_color ridge = top;
    ridge.a = (uint8_t)(ridge.a * 0.5f + 0.5f);
    cui_draw_line(ui, r0, r1, 1.4f, ridge);
}

/* A connection between two world points: a soft glow, a body and a bright
 * core, plus an optional travelling pulse dot. Drawn in the opaque pass so it
 * sits behind glass and in front of nothing it shouldn't. */
static void cui_iso__draw_link(cui_ctx *ui, const cui_iso_camera *cam,
                      const cui_iso_solid *o) {
    cui_vec2 a = cui_iso_project(cam, o->cx, o->cy, o->z);
    cui_vec2 b = cui_iso_project(cam, o->x2, o->y2, o->z2);
    float t = o->radius;
    /* Resolve before the strand is layered: a token's alpha is 0, so fading
     * the token's bytes drops the glow and the body, and the core would be
     * painted in the token's id bytes. */
    cui_color col = cui_color_of(ui, o->side);
    cui_color glow = col, body = col, core = col;
    glow.a = (uint8_t)(glow.a * CUI_ISO_STRAND_GLOW_A + 0.5f);
    body.a = (uint8_t)(body.a * CUI_ISO_STRAND_BODY_A + 0.5f);
    core.a = CUI_ISO_STRAND_CORE_A;
    cui_draw_line(ui, a, b, t * CUI_ISO_STRAND_GLOW_W, glow);
    cui_draw_line(ui, a, b, t * CUI_ISO_STRAND_BODY_W, body);
    cui_draw_line(ui, a, b, t * CUI_ISO_STRAND_CORE_W, core);

    if (o->pulse >= 0.0f && o->pulse <= 1.0f) {
        cui_vec2 p = { a.x + (b.x - a.x) * o->pulse, a.y + (b.y - a.y) * o->pulse };
        cui_color pd = core;
        pd.a = 255;
        cui_draw_circle(ui, p, t * 2.4f, (cui_color){ pd.r, pd.g, pd.b, 70 });
        cui_draw_circle(ui, p, t * 1.3f, pd);
    }


}

/* The caption floating above a link: drawn in the overlay pass, after every
 * solid, so nothing sorted in front of the link can paint over the letters.
 */
static void cui_iso__draw_link_label(cui_ctx *ui, const cui_iso_camera *cam,
                     const cui_iso_text_style *text, const cui_iso_solid *o) {
    if (!o->label) return;
    cui_vec2 a = cui_iso_project(cam, o->cx, o->cy, o->z);
    cui_vec2 b = cui_iso_project(cam, o->x2, o->y2, o->z2);
    cui_vec2 m = { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f - o->radius * 3.0f - 8.0f };
    char val[32];
    val[0] = 0;
    /* A negative value means "label only": the caller has no number to show.
     * Testing the decimals instead would print a number for a label that was
     * asked to carry none. */
    if (o->label_value >= 0.0f && o->label_decimals >= 0)
        snprintf(val, sizeof val, "%.*f", o->label_decimals, o->label_value);
    cui_draw_text(ui, text->font, text->size, m,
                  iso__label_color(ui, o->label_color, text),
                  o->label, (int)strlen(o->label));
    if (val[0]) {
        cui_text_metrics lm = cui_measure_text(ui, text->font, text->size,
                                               o->label, (int)strlen(o->label));
        cui_vec2 vp = { m.x + lm.width + 5.0f, m.y };
        cui_draw_text(ui, text->font, text->size, vp,
                      iso__label_color(ui, text->value_color, text),
                      val, (int)strlen(val));
    }
}

/* A curved conduit: a quadratic curve from A to B whose midpoint is lifted by
 * `bulge` toward the camera and up. Sampled into a polyline and drawn with the
 * same glow/body/core layering as a straight link, so the two read as the same
 * material. The pulse dot rides the curve, not the chord. */
static void cui_iso__draw_arc(cui_ctx *ui, const cui_iso_camera *cam,
                     const cui_iso_solid *o) {
    cui_vec2 a = cui_iso_project(cam, o->cx, o->cy, o->z);
    cui_vec2 b = cui_iso_project(cam, o->x2, o->y2, o->z2);
    /* control point: midpoint lifted in screen space by the bulge */
    float lift = o->chamfer * cam->scale;
    cui_vec2 ctrl = { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f - lift };
    float t = o->radius;

    enum { N = 24 };
    cui_vec2 path[N];
    for (int i = 0; i < N; i++) {
        float u = (float)i / (float)(N - 1);
        float v = 1.0f - u;
        path[i].x = v * v * a.x + 2.0f * v * u * ctrl.x + u * u * b.x;
        path[i].y = v * v * a.y + 2.0f * v * u * ctrl.y + u * u * b.y;
    }
    /* Resolve before the strand is layered: a token's alpha is 0, so fading
     * the token's bytes drops the glow and the body, and the core would be
     * painted in the token's id bytes. */
    cui_color col = cui_color_of(ui, o->side);
    cui_color glow = col, body = col, core = col;
    glow.a = (uint8_t)(glow.a * CUI_ISO_STRAND_GLOW_A + 0.5f);
    body.a = (uint8_t)(body.a * CUI_ISO_STRAND_BODY_A + 0.5f);
    core.a = CUI_ISO_STRAND_CORE_A;
    cui_draw_polyline(ui, path, N, t * CUI_ISO_STRAND_GLOW_W, glow, false);
    cui_draw_polyline(ui, path, N, t * CUI_ISO_STRAND_BODY_W, body, false);
    cui_draw_polyline(ui, path, N, t * CUI_ISO_STRAND_CORE_W, core, false);

    if (o->pulse >= 0.0f && o->pulse <= 1.0f) {
        /* index along the sampled curve */
        float fi = o->pulse * (float)(N - 1);
        int i0 = (int)fi;
        if (i0 > N - 2) i0 = N - 2;
        float f = fi - (float)i0;
        cui_vec2 p = { path[i0].x + (path[i0 + 1].x - path[i0].x) * f,
                       path[i0].y + (path[i0 + 1].y - path[i0].y) * f };
        cui_color pd = core;
        pd.a = 255;
        cui_draw_circle(ui, p, t * 2.4f, (cui_color){ pd.r, pd.g, pd.b, 70 });
        cui_draw_circle(ui, p, t * 1.3f, pd);
    }
}

/* Painter key for a solid, for ordering the draw pass. Links and arcs sort at
 * their midpoint so they sit between the two things they join; everything else
 * sorts at its own anchor. */
static float cui_iso__key(const cui_iso_camera *cam, const cui_iso_solid *o) {
    if (o->kind == CUI_ISO_LINK || o->kind == CUI_ISO_ARC)
        return cui_iso_depth(cam, (o->cx + o->x2) * 0.5f, (o->cy + o->y2) * 0.5f);
    return cui_iso_depth(cam, o->cx, o->cy);
}

void cui_iso_end(cui_iso_scene *s, cui_ctx *ui) {
    if (!s || !ui || !s->begun) return;

    /* Reduced motion: freeze every travelling pulse at rest. Only a pulse
     * that is actually travelling is frozen: a value outside 0..1 is how a
     * caller says "no pulse", and rewriting it to 0.5 would switch the pulse
     * on for a scene that asked for none. */
    if (s->reduced_motion)
        for (int i = 0; i < s->count; i++)
            if (s->solids[i].pulse >= 0.0f && s->solids[i].pulse <= 1.0f)
                s->solids[i].pulse = 0.5f;

    /* A diorama hosted in a panel must not paint outside it: when bounds are
     * set, every pass below is clipped to that rectangle until the pop at the
     * end of this function. The checked push says whether the clip was taken:
     * CUI's clip stack is finite, and a refused push would leave the scene
     * unclipped (painting over the neighbouring widgets) while the pop at the
     * end would take a clip that was never pushed. */
    bool clipped = false;
    if (s->has_bounds) {
        clipped = cui_push_clip_checked(ui, s->bounds);
        if (!clipped) {
            s->begun = false;
            return;
        }
    }
    /* The clip that is really in effect: the scene's bounds intersected with
     * the host's own clip. Picking has to agree with it, or a click lands on
     * a solid the host clipped away. It is recorded here, after a successful
     * push: a refused push painted nothing, so the last frame's clip is still
     * the truth about what is on screen. */
    s->painted_clip = cui_current_clip(ui);
    s->has_painted_clip = true;

    /* The ground first: shadows and the grid are a floor, never occluders. */
    for (int i = 0; i < s->count; i++)
        if (s->solids[i].kind == CUI_ISO_SHADOW)
            cui_iso__draw_shadow(ui, &s->cam, &s->solids[i]);

    /* Opaque solids back to front. Each key is computed once here rather than
     * once per comparison: at the cap that is 1024 sin/cos pairs, not half a
     * million. Strict > keeps the sort stable, so equal depths stay in the order
     * they were added and a frame is reproducible run to run. */
    int order[CUI_ISO_MAX_SOLIDS];
    float key[CUI_ISO_MAX_SOLIDS];
    int n = 0;
    for (int i = 0; i < s->count; i++) {
        cui_iso_kind k = s->solids[i].kind;
        if (k == CUI_ISO_SHADOW || k == CUI_ISO_GLASS || k == CUI_ISO_TEXT) continue;
        order[n] = i;
        key[n] = cui_iso__key(&s->cam, &s->solids[i]) + s->solids[i].bias;
        n++;
    }
    for (int i = 1; i < n; i++) {
        int ci = order[i];
        float ck = key[i];
        int j = i - 1;
        while (j >= 0 && key[j] > ck) { order[j + 1] = order[j]; key[j + 1] = key[j]; j--; }
        order[j + 1] = ci;
        key[j + 1] = ck;
    }
    for (int k = 0; k < n; k++) {
        cui_iso_solid *o = &s->solids[order[k]];
        switch (o->kind) {
            case CUI_ISO_BOX:      cui_iso__draw_box(ui, &s->cam, &s->light, o); break;
            case CUI_ISO_CYLINDER: cui_iso__draw_cylinder(ui, &s->cam, &s->light, o); break;
            case CUI_ISO_WEDGE:    cui_iso__draw_wedge(ui, &s->cam, &s->light, o); break;
            case CUI_ISO_SPHERE:   cui_iso__draw_sphere(ui, &s->cam, o); break;
            case CUI_ISO_LINK:     cui_iso__draw_link(ui, &s->cam, o); break;
            case CUI_ISO_ARC:      cui_iso__draw_arc(ui, &s->cam, o); break;
            default: break;
        }
    }

    /* Glass sits over what it holds, so it comes after every opaque solid. */
    for (int i = 0; i < s->count; i++)
        if (s->solids[i].kind == CUI_ISO_GLASS)
            cui_iso__draw_glass(ui, &s->cam, &s->solids[i]);

    /* Text last, in one overlay pass. Drawn inside the sorted pass a caption is
     * just another solid, so anything sorted in front of it paints over the
     * letters and the label becomes unreadable at exactly the depths where it
     * matters. A caption is an annotation, not geometry: it goes on top. */
    for (int i = 0; i < s->count; i++) {
        cui_iso_solid *o = &s->solids[i];
        if (o->kind == CUI_ISO_TEXT)
            cui_iso__draw_text(ui, &s->cam, &s->text, o);
        else if (o->kind == CUI_ISO_LINK)
            cui_iso__draw_link_label(ui, &s->cam, &s->text, o);
    }

    if (clipped) cui_pop_clip(ui);

    s->begun = false;
}

void cui_iso_depth_bias_last(cui_iso_scene *sc, float bias) {
    /* Only inside a batch: after cui_iso_end the frame is painted, and a bias
     * written then would change what cui_iso_pick reports without changing
     * what is on screen. */
    if (sc && sc->begun && sc->count > 0) sc->solids[sc->count - 1].bias = bias;
}

/* A tone t of the way from the ground toward the ink, opaque.
 *
 * Why a ramp between background and foreground, rather than CUI_COLOR_SURFACE
 * and CUI_COLOR_SURFACE_RAISED? Measured on this CUI revision: in the light
 * theme both resolve to 255,255,255, so a plate and the block standing on it
 * are the same colour and the diorama disappears. A blend toward that theme's
 * own ink gives the same relative separation in either theme, stays in the
 * theme's colour family (no second palette), and lands on the same side of the
 * background as text does.
 *
 * Opaque throughout: cui_iso_shade multiplies RGB, and a token sentinel has
 * alpha 0, so a token must be resolved here rather than carried into a solid.
 * The blend is cui_mix, CUI's own perceptual one, not a second implementation.
 */
static cui_color iso__tone(cui_color ground, cui_color ink, float t) {
    return cui_mix(ground, ink, t);
}

cui_iso_palette cui_iso_palette_default(cui_ctx *ui) {
    cui_iso_palette p;
    if (!ui) {
        /* No context: a mid grey family, wrong for every theme but visible in
         * all of them. Only reachable when a caller passes a NULL ui by hand. */
        p.plate = p.block = p.floor = (cui_color){ 0x80, 0x80, 0x80, 255 };
        p.plate_side = p.block_side = (cui_color){ 0x60, 0x60, 0x60, 255 };
        p.glass = (cui_color){ 0x80, 0x80, 0x80, 60 };
        p.ink = (cui_color){ 0x10, 0x10, 0x10, 255 };
        p.ink_dim = (cui_color){ 0x50, 0x50, 0x50, 255 };
    } else {
        cui_color ground = cui_color_of(ui, CUI_COLOR_BACKGROUND);
        cui_color ink = cui_color_of(ui, CUI_COLOR_FOREGROUND);
        /* The ramp reads as depth: the further a face is from the top, the more
         * of the ink it carries. Steps of 6/4/2 per level stay visible on both a
         * near-white and a near-black ground without reaching either extreme. */
        p.floor      = iso__tone(ground, ink, 0.020f);
        p.plate      = iso__tone(ground, ink, 0.075f);
        p.plate_side = iso__tone(ground, ink, 0.135f);
        p.block      = iso__tone(ground, ink, 0.110f);
        p.block_side = iso__tone(ground, ink, 0.190f);
        p.glass      = iso__tone(ground, ink, 0.060f);
        p.glass.a    = 40;
        p.ink        = ink;
        p.ink_dim    = cui_color_of(ui, CUI_COLOR_TEXT_MUTED);
    }

    /* Status hues are fixed on purpose: "failed" must read as failure in
     * either theme, so they are not derived from the ground or the accent. */
    p.ok     = (cui_color){ 0x34, 0xd1, 0x8c, 255 };
    p.warn   = (cui_color){ 0xf5, 0xa5, 0x24, 255 };
    p.bad    = (cui_color){ 0xe5, 0x48, 0x4d, 255 };
    p.info   = (cui_color){ 0x4f, 0xa3, 0xe0, 255 };
    p.accent = ui ? cui_color_of(ui, CUI_COLOR_ACCENT) : p.info;
    return p;
}

/* ---- picking ------------------------------------------------------------- */

static float iso__cross(cui_vec2 o, cui_vec2 a, cui_vec2 b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

/* Convex hull of n points by Andrew monotone chain, CCW. Writes the hull back
 * into pts and returns its length (0 if fewer than 3 distinct points).
 *
 * The scratch buffer has to hold every input point: a cylinder hulls 2 * 64 rim
 * samples, so the buffer is sized for that, not for the old 8-point box. The
 * monotone chain keeps the lower and the upper hull in one array while it
 * builds them, so the buffer is twice the point count. */
static int iso__hull(cui_vec2 *p, int n)
{
    if (n < 3 || n > CUI_ISO_MAX_HULL_PTS) return 0;
    for (int i = 1; i < n; i++) {
        cui_vec2 c = p[i];
        int j = i - 1;
        while (j >= 0 && (p[j].x > c.x || (p[j].x == c.x && p[j].y > c.y))) {
            p[j + 1] = p[j];
            j--;
        }
        p[j + 1] = c;
    }
    cui_vec2 h[CUI_ISO_MAX_HULL_PTS * 2];   /* must hold every input point */
    int k = 0;
    for (int i = 0; i < n; i++) {
        while (k >= 2 && iso__cross(h[k - 2], h[k - 1], p[i]) <= 0.0f) k--;
        h[k++] = p[i];
    }
    int low = k + 1;
    for (int i = n - 2; i >= 0; i--) {
        while (k >= low && iso__cross(h[k - 2], h[k - 1], p[i]) <= 0.0f) k--;
        h[k++] = p[i];
    }
    if (k > 0) k--;
    for (int i = 0; i < k; i++) p[i] = h[i];
    return k;
}

static bool iso__in_hull(const cui_vec2 *pts, int n, float sx, float sy)
{
    bool neg = false, pos = false;
    for (int i = 0; i < n; i++) {
        cui_vec2 a = pts[i], b = pts[(i + 1) % n];
        float cr = (b.x - a.x) * (sy - a.y) - (b.y - a.y) * (sx - a.x);
        if (cr < -1e-4f) neg = true;
        else if (cr > 1e-4f) pos = true;
    }
    return !(neg && pos);
}

/* A box paints a prism from its lid down to z_top - height, so a click must land
 * anywhere on that solid, not only on the lid. Under the orthographic projection
 * a convex prism stays convex on screen, and its silhouette is exactly the convex
 * hull of its eight projected corners; test the point against that hull.
 */

/* A click on the lid: the projected top face of the box, no body. */
bool cui_iso_hit_box(const cui_iso_camera *cam, float sx, float sy,
                    float cx, float cy, float z_top, float w, float d)
{
    /* A zero scale projects every world point onto the same screen point, so
     * the hull degenerates to a point and every click would land on it. */
    if (!cam || cam->scale == 0.0f) return false;
    float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
    float y0 = cy - d * 0.5f, y1 = cy + d * 0.5f;
    cui_vec2 q[4] = {
        cui_iso_project(cam, x0, y0, z_top),
        cui_iso_project(cam, x1, y0, z_top),
        cui_iso_project(cam, x1, y1, z_top),
        cui_iso_project(cam, x0, y1, z_top),
    };
    bool neg = false, pos = false;
    for (int i = 0; i < 4; i++) {
        cui_vec2 a = q[i], b = q[(i + 1) % 4];
        float cr = (b.x - a.x) * (sy - a.y) - (b.y - a.y) * (sx - a.x);
        if (cr < -1e-6f) neg = true;
        else if (cr > 1e-6f) pos = true;
    }
    return !(neg && pos);
}

bool cui_iso_hit_solid(const cui_iso_camera *cam, float sx, float sy,
                     float cx, float cy, float z_top, float height,
                     float w, float d)
{
    if (!cam || cam->scale == 0.0f) return false;
    float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
    float y0 = cy - d * 0.5f, y1 = cy + d * 0.5f;
    float zt = z_top, zb = z_top - height;
    const float kx[4] = { x0, x1, x1, x0 };
    const float ky[4] = { y0, y0, y1, y1 };
    cui_vec2 p[8];
    for (int i = 0; i < 4; i++) {
        p[i]     = cui_iso_project(cam, kx[i], ky[i], zt);
        p[i + 4] = cui_iso_project(cam, kx[i], ky[i], zb);
    }
    int n = iso__hull(p, 8);
    if (n < 3) return false;
    bool neg = false, pos = false;
    for (int i = 0; i < n; i++) {
        cui_vec2 a = p[i], b = p[(i + 1) % n];
        float cr = (b.x - a.x) * (sy - a.y) - (b.y - a.y) * (sx - a.x);
        if (cr < -1e-6f) neg = true;
        else if (cr > 1e-6f) pos = true;
    }
    return !(neg && pos);
}

/* A sphere is a screen-space circle at its projected centre. */
bool cui_iso_hit_sphere(const cui_iso_camera *cam, float sx, float sy,
                        float x, float y, float z, float radius) {
    if (!cam || cam->scale == 0.0f) return false;
    cui_vec2 c = cui_iso_project(cam, x, y, z);
    float r = radius * cam->scale;
    float dx = sx - c.x, dy = sy - c.y;
    return dx * dx + dy * dy <= r * r;
}

/* The rim of a cylinder, sampled the way the renderer builds it: `sides`
 * vertices at i/sides * 2pi, the top rim and the bottom rim. The hit test and
 * the renderer have to agree on those vertices -- sampling the rim any other
 * way makes the clickable area differ from the painted shape (an 8-sided
 * cylinder at 315 degrees loses a visible vertex). */
static int iso__cyl_rim(const cui_iso_camera *cam, float cx, float cy,
                        float z_bottom, float radius, float height,
                        int sides, cui_vec2 *q)
{
    if (sides < 8) sides = 8;
    if (sides > CUI_ISO_MAX_CYL_SIDES) sides = CUI_ISO_MAX_CYL_SIDES;
    for (int i = 0; i < sides; i++) {
        float a = (float)i / (float)sides * 2.0f * CUI_PI;
        float ux = cosf(a) * radius, uy = sinf(a) * radius;
        q[i] = cui_iso_project(cam, cx + ux, cy + uy, z_bottom + height);
        q[i + sides] = cui_iso_project(cam, cx + ux, cy + uy, z_bottom);
    }
    return iso__hull(q, 2 * sides);
}

/* A cylinder reads as its projected top ellipse plus the body below it, so a
 * click lands if it is inside the top ellipse or in the column beneath it.
 *
 * The silhouette is the convex hull of the two rims, sampled with the same
 * vertex count the renderer uses. This form takes no side count, so it tests
 * the roundest cylinder the renderer can draw (64 sides); a coarser cylinder
 * has a slightly smaller footprint, and picking one exactly needs
 * cui_iso_pick, which reads the side count off the solid. */
bool cui_iso_hit_cylinder(const cui_iso_camera *cam, float sx, float sy,
                          float cx, float cy, float z_bottom, float radius,
                          float height) {
    if (!cam || cam->scale == 0.0f || radius <= 0.0f) return false;
    cui_vec2 q[2 * CUI_ISO_MAX_CYL_SIDES];
    int n = iso__cyl_rim(cam, cx, cy, z_bottom, radius, height,
                         CUI_ISO_MAX_CYL_SIDES, q);
    return n >= 3 && iso__in_hull(q, n, sx, sy);
}

/* Pick the front-most solid under a screen point, by the same depth key the
 * sort uses. Returns the solid's index in the batch, or -1 when nothing is hit.
 *
 * Works during a batch and after cui_iso_end: end keeps the camera and the count
 * (it only clears begun), so a click handler can ask "what did I just draw here?"
 * without threading the camera through. Boxes and cylinders are tested by
 * volume, spheres by their screen circle.
 */
/* A box paints a prism from its lid down to z_top - height with the
 * chamfered footprint the renderer uses, so a click has to land on that same
 * chamfered silhouette: the un-chamfered rectangle is bigger than the painted
 * shape at the corners, and picking answers on pixels nothing was drawn on. */
static bool iso__hit_box_volume(const cui_iso_camera *cam, float sx, float sy,
                                const cui_iso_solid *o) {
    if (!cam || cam->scale == 0.0f) return false;
    float x0 = o->cx - o->w * 0.5f, x1 = o->cx + o->w * 0.5f;
    float y0 = o->cy - o->d * 0.5f, y1 = o->cy + o->d * 0.5f;
    float c = o->chamfer;
    float wx[8] = { x0 + c, x1 - c, x1,     x1,     x1 - c, x0 + c, x0,     x0     };
    float wy[8] = { y0,     y0,     y0 + c, y1 - c, y1,     y1,     y1 - c, y0 + c };
    float zt = o->z, zb = o->z - o->height;
    cui_vec2 p[16];
    for (int i = 0; i < 8; i++) {
        p[i] = cui_iso_project(cam, wx[i], wy[i], zt);
        p[i + 8] = cui_iso_project(cam, wx[i], wy[i], zb);
    }
    int n = iso__hull(p, 16);
    return n >= 3 && iso__in_hull(p, n, sx, sy);
}

int cui_iso_pick(cui_iso_scene *s, float sx, float sy) {
    if (!s) return -1;
    /* Picking answers about the pixels that were painted, so it uses the clip
     * of the last successful paint -- the scene's bounds intersected with the
     * host's own clip. A click outside it selects nothing; a solid that is
     * only partly clipped stays selectable on its visible part, because the
     * test rejects the point, not the solid.
     *
     * The pending bounds are only a fallback, for a scene that has never been
     * painted. Changing the bounds after a paint does not change what is on
     * screen, so it must not change what a click selects either. */
    if (s->has_painted_clip) {
        cui_rect r = s->painted_clip;
        if (sx < r.x || sy < r.y || sx > r.x + r.w || sy > r.y + r.h) return -1;
    } else if (s->has_bounds) {
        cui_rect r = s->bounds;
        if (sx < r.x || sy < r.y || sx > r.x + r.w || sy > r.y + r.h) return -1;
    }
    int best = -1;
    float best_key = 0.0f;
    for (int i = 0; i < s->count; i++) {
        cui_iso_solid *o = &s->solids[i];
        bool hit = false;
        switch (o->kind) {
            case CUI_ISO_BOX:
                hit = iso__hit_box_volume(&s->cam, sx, sy, o);
                break;
            case CUI_ISO_CYLINDER: {
                cui_vec2 q[2 * CUI_ISO_MAX_CYL_SIDES];
                int n = iso__cyl_rim(&s->cam, o->cx, o->cy, o->z, o->radius,
                                     o->height, (int)o->chamfer, q);
                hit = n >= 3 && iso__in_hull(q, n, sx, sy);
                break;
            }
            case CUI_ISO_SPHERE:
                hit = cui_iso_hit_sphere(&s->cam, sx, sy, o->cx, o->cy, o->z,
                                        o->radius);
                break;
            default:
                break;
        }
        if (hit) {
            float k = cui_iso__key(&s->cam, o) + o->bias;
            /* >= so the later insertion wins a tie: the sort paints it on top,
             * so picking the first hit at an equal key would name one solid and
             * show another. */
            if (best < 0 || k >= best_key) { best = i; best_key = k; }
        }
    }
    return best;
}
