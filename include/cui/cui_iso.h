/* cui_iso.h - a small isometric diorama layer for CUI.
 *
 * CUI draws flat 2D primitives (cui_draw_polygon, cui_draw_polyline,
 * cui_draw_circle, cui_draw_line). There is no depth buffer, no mesh, no 3D
 * API. This header adds the one thing you need to fake a 3D diorama on top of
 * those primitives, the way mcpd-ui does it by hand:
 *
 *   - an orthographic isometric camera you can orbit (cui_iso_camera),
 *   - a projection from world space (x right, y forward, z up) to CUI screen
 *     space (cui_iso_project / cui_iso_depth),
 *   - a depth-sorted list of opaque and glassy solids drawn back to front, so
 *     you never hand-order a scene (cui_iso_begin / cui_iso_box_named /
 *     cui_iso_end),
 *   - per-face lighting against one fixed world light, so boxes read as solids
 *     and not as flat rhombi (cui_iso_light).
 *
 * Everything is drawn with plain cui_draw_* calls: no WebGL, no assets, no
 * extra renderer. A diorama is therefore a normal CUI paint function, which
 * means it works on every CUI backend (software, OpenGL, WebAssembly) and can
 * sit next to regular 2D widgets in the same frame.
 *
 * Coordinate convention (matches mcpd-ui):
 *   x = world right, y = world forward (away from camera at az = 0),
 *   z = world up. Units are arbitrary but consistent; a box of w = d = 100
 *   fills a comfortable share of a 760x620 viewport at scale 1.
 *
 * Screen-space output is a cui_vec2 in the coordinate space CUI's draw calls
 * expect, so you can mix projected points with cui_draw_text and friends.
 *
 * The solids themselves are declared in <cui/cui_iso_opts.h>, one options
 * struct and one named-field call per solid:
 *
 *   #include <cui/cui_iso_opts.h>
 *   cui_iso_box_named(sc, .cx = 0, .cy = 0, .z_top = 30, .w = 70, .d = 70,
 *                      .height = 60, .chamfer = 10, .top = a, .side = b);
 *
 * Not included on purpose: texture mapping, perspective, mesh loading, and any
 * hidden-surface removal beyond painter's algorithm over whole solids. If you
 * need those, use a real 3D renderer (see cui_render.h); this layer targets
 * crisp, vector-clean dioramas like control surfaces and status scenes.
 */
#ifndef CUI_ISO_H
#define CUI_ISO_H

#include <cui/cui.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- lighting ------------------------------------------------------------ */

/* How every label in a scene is drawn -- font, size, colour. Set once with
 * cui_iso_set_text_style; zero-initialise for the defaults (body font, 12 px,
 * white). Used by link labels and any face text. */
typedef struct cui_iso_text_style {
    cui_font font;         /* CUI_FONT_BODY / CUI_FONT_MEDIUM / ... */
    float size;            /* px; <=0 -> 12 */
    cui_color color;       /* 0 alpha -> white */
    cui_color value_color; /* colour for a numeric value beside a label; 0 alpha -> color */
} cui_iso_text_style;

/* One fixed world light. Surface normals are dotted against this to shade the
 * sides of a solid; the top face always gets the full top colour. LIGHT_X and
 * LIGHT_Y are the light's direction in the x/y plane; mcpd-ui uses
 * (-0.62, -0.78), which lights the two camera-facing sides. */
typedef struct cui_iso_light {
    float x, y;          /* light direction in the ground plane */
    float floor, ceil;   /* side brightness range: floor + (ceil-floor)*facing */
} cui_iso_light;

/* The default light, matching mcpd-ui's look. */
CUI_API cui_iso_light cui_iso_light_default(void);

/* Shade a colour by how much a side faces the light. `nx`/`ny` is the side's
 * outward normal in the ground plane (unit length not required). Returns the
 * colour scaled along the light's floor..ceil range. */
CUI_API cui_color cui_iso_shade(cui_color base, cui_iso_light light, float nx, float ny);

/* ---- camera -------------------------------------------------------------- */

/* An orbiting orthographic isometric camera. `az` is the orbit angle in
 * radians around the vertical axis; `cx`/`cy` is the screen point that world
 * origin (0,0,0) maps to; `scale` is pixels per world unit. */
typedef struct cui_iso_camera {
    float az;     /* orbit angle, radians */
    float cx, cy; /* screen position of world origin */
    float scale;  /* pixels per world unit */
} cui_iso_camera;

/* Project a world point to CUI screen space. */
CUI_API cui_vec2 cui_iso_project(const cui_iso_camera *cam, float x, float y, float z);

/* Painter's-algorithm depth of a world point: larger = closer to the camera.
 * Sort solids by this, ascending, and draw in that order. */
CUI_API float cui_iso_depth(const cui_iso_camera *cam, float x, float y);

/* The camera's light-facing helpers used internally; exposed so a custom solid
 * can match the built-ins' shading. */
CUI_API void cui_iso_basis(const cui_iso_camera *cam, float *cos_az, float *sin_az);

/* ---- slots: places the library works out for you ------------------------- */

/* A slot is a place in the scene. Instead of typing raw coordinates (and doing
 * trig in your head), you ask a layout helper for the slots and place solids
 * at them. Stack on top of a slot with cui_iso_on instead of adding heights by
 * hand. Most scenes then contain no coordinate numbers at all. */
typedef struct cui_iso_slot {
    float x, y, z;
} cui_iso_slot;

/* Raise a slot by `height` -- "put this on top of that." */
CUI_API cui_iso_slot cui_iso_on(cui_iso_slot s, float height);

/* The largest grid a layout will fill. The slot count has to fit in an int and
 * in the caller's buffer; a bigger grid is refused and returns 0. */
#define CUI_ISO_MAX_GRID_CELLS 4096

/* The three layouts, each taking its names instead of a positional run.
 * A layout is described by where it starts and how it steps, so the opts
 * structs say that: start and spacing, not five interchangeable floats.
 *
 *   cui_iso_layout_row_ex(out, 6, &(cui_iso_layout_row_opts){ .spacing = 30 });
 *
 * A row of `n` slots left-to-right, centred on (x, y) at height z, `spacing`
 * apart; slot 0 is the left-most.
 * A ring of `n` slots around (x, y), the first one at the front.
 * A grid of slots, `cols` across by `rows` deep, `cell` world units each.
 * Each returns how many slots it filled. A layout that cannot be filled -- a
 * non-positive count, a NULL `out`, or a grid with more than
 * CUI_ISO_MAX_GRID_CELLS cells -- returns 0 and writes nothing. */
typedef struct cui_iso_layout_row_opts {
    int _;
    float x, y, z;   /* centre of the row */
    float spacing;   /* centre-to-centre distance between slots */
} cui_iso_layout_row_opts;

typedef struct cui_iso_layout_ring_opts {
    int _;
    float x, y, z;   /* centre of the ring */
    float radius;    /* distance from the centre to a slot */
} cui_iso_layout_ring_opts;

typedef struct cui_iso_layout_grid_opts {
    int _;
    float x, y, z;   /* centre of the grid */
    float cell;      /* world units per cell, both axes */
} cui_iso_layout_grid_opts;

CUI_API int cui_iso_layout_row_ex(cui_iso_slot *out, int n,
                                  const cui_iso_layout_row_opts *o);
CUI_API int cui_iso_layout_ring_ex(cui_iso_slot *out, int n,
                                   const cui_iso_layout_ring_opts *o);
CUI_API int cui_iso_layout_grid_ex(cui_iso_slot *out, int cols, int rows,
                                   const cui_iso_layout_grid_opts *o);

/* ---- solid list ---------------------------------------------------------- */

/* A batch of solids drawn back to front. Fill it between cui_iso_begin and
 * cui_iso_end; nothing is drawn until cui_iso_end, which is what gives you
 * correct overlap without hand-sorting.
 *
 * The solids are declared in <cui/cui_iso_opts.h>, one named-field call each:
 * box, slab, glass, cylinder, sphere, shadow, grid, link, link_labeled, arc,
 * wedge, text, lattice and disk_stack.
 *
 * Opaque solids (the box) are sorted by depth and drawn first-to-last.
 * Glass solids (the vitrine) are drawn after all opaque ones at the same
 * footprint, on top, because they are translucent overlays (a vitrine over a
 * prop). Pulses/labels you draw yourself, after cui_iso_end.
 *
 * Create one with cui_iso_scene_create() and reuse it across frames; call
 * cui_iso_begin each frame, which resets the batch. Capacity is fixed at
 * CUI_ISO_MAX_SOLIDS, so the whole thing is one allocation with no realloc. */
typedef struct cui_iso_scene cui_iso_scene;

/* Capacity of a scene, in solids. Only the first CUI_ISO_MAX_SOLIDS solids
 * added between begin and end are drawn; the rest are dropped, not overwritten. */
#define CUI_ISO_MAX_SOLIDS 1024

CUI_API cui_iso_scene *cui_iso_scene_create(void);
CUI_API void cui_iso_scene_destroy(cui_iso_scene *s);
CUI_API void cui_iso_scene_reset(cui_iso_scene *s);
/* How many solids the current batch holds. Never more than CUI_ISO_MAX_SOLIDS:
 * once the batch is full further solids are dropped, so a caller can tell an
 * overflow happened by comparing what it added with what is held. */
CUI_API int cui_iso_scene_count(const cui_iso_scene *s);
/* Clip every solid to this rectangle on screen, e.g. the canvas a diorama is
 * painted into, so a large scale cannot bleed over neighbouring widgets. End
 * pushes the clip before its first solid and pops it after its last, so the
 * calls pair. Unset is the default: draw anywhere. A zero or negative width or
 * height clears the clip, so the same scene can stop drawing to a canvas. */
CUI_API void cui_iso_set_bounds(cui_iso_scene *s, cui_rect r);

/* Replace the scene's light. The default matches the mcpd look; flip x and y
 * to light a scene from the other side, or lower ceil to flatten the shading. */
CUI_API void cui_iso_set_light(cui_iso_scene *s, cui_iso_light light);

/* Start a frame's solid batch against this camera. A NULL camera invalidates
 * the batch instead of leaving the previous one alive: the next cui_iso_end
 * draws nothing rather than the last frame's solids. */
CUI_API void cui_iso_begin(cui_iso_scene *s, const cui_iso_camera *cam);

/* Set how every label in the scene is drawn (font, size, colour). Call once
 * after creating the scene; labels added later pick it up. Zero-initialise a
 * cui_iso_text_style for the defaults (body font, 12 px, white). */
CUI_API void cui_iso_set_text_style(cui_iso_scene *s, cui_iso_text_style style);

/* A link from slot `a` to slot `b`, trimmed in by `gap` world units at each end
 * so it meets the edge of whatever sits on the slot instead of its centre. This
 * is the no-magic-numbers way to connect two things: you never work out where
 * the edge is. */
CUI_API void cui_iso_link_between(cui_iso_scene *s, cui_iso_slot a, cui_iso_slot b,
                                  float gap, float thickness, cui_color color,
                                  float pulse);

/* A caption anchored at a world point, centred on it. Captions are drawn in a
 * final overlay pass after every solid, so a solid never paints over a
 * caption. Pass a colour with alpha 0 to use the scene's text colour.
 *
 * The string is not copied: keep it valid until cui_iso_end returns (or until
 * the batch is discarded), or the caption reads freed memory. */
CUI_API void cui_iso_text(cui_iso_scene *s, float x, float y, float z,
                          const char *text, cui_color color);

/* CUI's reduced-motion path: when on, travelling pulses freeze at rest instead
 * of animating, so a motion-sensitive user still gets a complete scene. Call
 * once after creating the scene (wire it to CUI's own reduced-motion setting). */
CUI_API void cui_iso_set_reduced_motion(cui_iso_scene *s, bool on);

/* Place `count` solids evenly around a circle of `radius` at height z -- the
 * hub-and-spoke layout. For each position it calls `place` with the slot index,
 * the world x/y and a 0..1 angle fraction; you draw the solid that belongs
 * there. When `conduit` is true an arc is drawn from the centre to each slot,
 * pulsing in sequence (t drives the animation, frozen under reduced motion). */
CUI_API void cui_iso_ring(cui_iso_scene *s, float cx, float cy, float z, float radius,
                          int count, float t, bool conduit, cui_color conduit_color,
                          void (*place)(cui_iso_scene *s, int index, float x, float y,
                                        float angle_frac, void *user),
                          void *user);

/* Draw the whole batch back to front into a CUI context. Call this inside your
 * paint function. */
CUI_API void cui_iso_end(cui_iso_scene *s, cui_ctx *ui);

/* Nudge the solid added last toward (+) or away from (-) the camera when the
 * scene is sorted for drawing. The sort is by depth, which a diorama gets right;
 * a bias is for the tie the geometry cannot decide, such as a conduit that
 * should read in front of the platform it grazes.
 *
 * Only inside a batch: after cui_iso_end the frame is already painted, and a
 * bias written then would change what cui_iso_pick reports without changing
 * what is on screen. */
CUI_API void cui_iso_depth_bias_last(cui_iso_scene *sc, float bias);

/* ---- picking ------------------------------------------------------------- */

/* Click picking. Each returns true if the screen point (sx, sy) lands on that
 * solid. A box is tested on its projected LID; the body below the lid needs
 * cui_iso_hit_solid. A sphere is tested by its screen circle, a cylinder by
 * its top ellipse plus the column beneath it. */
CUI_API bool cui_iso_hit_box(const cui_iso_camera *cam, float sx, float sy,
                             float cx, float cy, float z_top, float w, float d);
/* The whole box volume, lid down to z_top - height: what cui_iso_pick uses, so a
 * click on the body of a box selects it, not only a click on its lid. */
CUI_API bool cui_iso_hit_solid(const cui_iso_camera *cam, float sx, float sy,
                               float cx, float cy, float z_top, float height,
                               float w, float d);
CUI_API bool cui_iso_hit_sphere(const cui_iso_camera *cam, float sx, float sy,
                                float x, float y, float z, float radius);
CUI_API bool cui_iso_hit_cylinder(const cui_iso_camera *cam, float sx, float sy,
                                  float cx, float cy, float z_bottom, float radius,
                                  float height);

/* Pick the front-most solid under a screen point. Returns its index in the
 * order the solids were submitted, or -1 when nothing is hit -- so the answer
 * maps straight back to the call that added the solid. Boxes and cylinders are
 * tested by volume, spheres by their screen circle. Safe to call after
 * cui_iso_end: the scene keeps the camera it was drawn with, so a click handler
 * can ask what was just drawn there. */
CUI_API int cui_iso_pick(cui_iso_scene *s, float sx, float sy);

/* ---- palette -------------------------------------------------------------
 *
 * A theme-aware palette for dioramas. The neutral entries (plate, plate_side,
 * block, block_side, glass, ink, ink_dim, floor) are CUI colour tokens, so they
 * resolve per theme and one scene reads correctly in light and in dark. The
 * semantic entries (ok, warn, bad, info, accent) are fixed hues, because a
 * status means the same thing in either theme and must not drift.
 *
 * Most callers should use this instead of raw CUI_RGB(0x..) literals:
 *
 *   #include <cui/cui_iso_opts.h>
 *   cui_iso_palette pal = cui_iso_palette_default(ui);
 *   cui_iso_box_named(sc, .w = 70, .d = 70, .height = 60, .chamfer = 10,
 *                      .top = pal.block, .side = pal.block_side);
 */
typedef struct cui_iso_palette {
    cui_color plate, plate_side;    /* a platform: top face, shaded sides */
    cui_color block, block_side;    /* a raised block */
    cui_color glass;                /* a vitrine tint, alpha included */
    cui_color ink, ink_dim;         /* text, secondary text */
    cui_color floor;                /* ground surface under a grid */
    cui_color ok, warn, bad;        /* status: passed, running, failed */
    cui_color info, accent;         /* data, emphasis */
} cui_iso_palette;

CUI_API cui_iso_palette cui_iso_palette_default(cui_ctx *ui);

#ifdef __cplusplus
}
#endif

#endif /* CUI_ISO_H */
