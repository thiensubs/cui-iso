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
 *     you never hand-order a scene (cui_iso_begin / cui_iso_box / cui_iso_end),
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

/* How every label in a scene is drawn — font, size, colour. Set once with
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

/* Raise a slot by `height` — "put this on top of that." */
CUI_API cui_iso_slot cui_iso_on(cui_iso_slot s, float height);

/* A row of `n` slots left-to-right, centred on (cx, cy) at height z, `spacing`
 * apart. Slot 0 is the left-most. Returns how many it filled (n). */
CUI_API int cui_iso_layout_row(cui_iso_slot *out, int n, float cx, float cy,
                               float z, float spacing);

/* A ring of `n` slots around (cx, cy), first one at the front. */
CUI_API int cui_iso_layout_ring(cui_iso_slot *out, int n, float cx, float cy,
                                float z, float radius);

/* A grid of slots: `cols` across by `rows` deep, `cell` world units each. */
CUI_API int cui_iso_layout_grid(cui_iso_slot *out, int cols, int rows,
                                float cx, float cy, float z, float cell);

/* ---- solid list ---------------------------------------------------------- */

/* A batch of solids drawn back to front. Fill it between cui_iso_begin and
 * cui_iso_end; nothing is drawn until cui_iso_end, which is what gives you
 * correct overlap without hand-sorting.
 *
 * Opaque solids (cui_iso_box) are sorted by depth and drawn first-to-last.
 * Glass solids (cui_iso_glass) are drawn after all opaque ones at the same
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

/* Start a frame's solid batch against this camera. */
CUI_API void cui_iso_begin(cui_iso_scene *s, const cui_iso_camera *cam);

/* A chamfered box: a rectangular prism with its vertical corners cut by
 * `chamfer` world units, which is what makes the silhouette read as 3D rather
 * than a flat hexagon. (cx, cy) is the footprint centre; the box sits with its
 * TOP at z = z_top and extends down `height`. `top` is the top-face colour;
 * `side` is the base side colour, shaded per face by the scene's light. */
CUI_API void cui_iso_box(cui_iso_scene *s, float cx, float cy, float z_top,
                         float w, float d, float height, float chamfer,
                         cui_color top, cui_color side);

/* A thin flat slab, convenience for a box of small height (a platform). */
CUI_API void cui_iso_slab(cui_iso_scene *s, float cx, float cy, float z_top,
                          float w, float d, float height, cui_color top,
                          cui_color side);

/* A translucent glass case over a footprint, with a neon frame on its top
 * edges. Drawn after opaque solids. `tint` colours the glass panels; the frame
 * uses `tint` at higher alpha. */
CUI_API void cui_iso_glass(cui_iso_scene *s, float cx, float cy, float z_bottom,
                           float w, float d, float height, cui_color tint);

/* A vertical cylinder standing on z_bottom: pillars, tanks, columns, pipes.
 * `sides` is the tessellation (clamp 8..64; 24 looks round at diorama scale).
 * The side is lit strip-by-strip like a box, so it shades consistently with the
 * boxes around it. */
CUI_API void cui_iso_cylinder(cui_iso_scene *s, float cx, float cy, float z_bottom,
                              float radius, float height, int sides,
                              cui_color top, cui_color side);

/* A depth-sorted ball at a world point: molecule atoms, node markers,
 * particles. Drawn with a top-left highlight so it reads as a sphere. */
CUI_API void cui_iso_sphere(cui_iso_scene *s, float x, float y, float z,
                            float radius, cui_color color);

/* A connection between two world points — the "sense flow" primitive: conduits,
 * pipes, edges in a topology or pipeline. Drawn as a soft glow + body + bright
 * core. `pulse` in 0..1 places a travelling dot at that fraction along the link
 * (pass <0 or >1 for no pulse), so a scene can animate flow by moving one value
 * each frame. Sorted at the link's midpoint, so it sits between the solids it
 * joins. */
CUI_API void cui_iso_link(cui_iso_scene *s, float x1, float y1, float z1,
                          float x2, float y2, float z2, float thickness,
                          cui_color color, float pulse);

/* Set how every label in the scene is drawn (font, size, colour). Call once
 * after creating the scene; labels added later pick it up. Zero-initialise a
 * cui_iso_text_style for the defaults (body font, 12 px, white). */
CUI_API void cui_iso_set_text_style(cui_iso_scene *s, cui_iso_text_style style);

/* cui_iso_link plus a label and a live number floating at the midpoint — the
 * "LLM --3.2 tok/s--> Optimize" look from pipeline art. `decimals` is digits
 * after the point for `value` (pass 0 for "12", 1 for "3.2"); a negative value
 * draws the label with no number. The label uses the scene's text style. */
CUI_API void cui_iso_link_labeled(cui_iso_scene *s, float x1, float y1, float z1,
                                  float x2, float y2, float z2, float thickness,
                                  cui_color color, float pulse,
                                  const char *label, float value, int decimals);

/* A curved conduit that bulges above the straight path — the ring conduits and
 * curved flow arrows in pipeline art. `bulge` is how far (world units) the
 * midpoint lifts toward the camera and up; 0 collapses to a straight link.
 * Draws with the same glow/body/core layering as cui_iso_link. */
CUI_API void cui_iso_arc(cui_iso_scene *s, float x1, float y1, float z1,
                         float x2, float y2, float z2, float bulge, float thickness,
                         cui_color color, float pulse);

/* A link from slot `a` to slot `b`, trimmed in by `gap` world units at each end
 * so it meets the edge of whatever sits on the slot instead of its centre. This
 * is the no-magic-numbers way to connect two things: you never work out where
 * the edge is. */
CUI_API void cui_iso_link_between(cui_iso_scene *s, cui_iso_slot a, cui_iso_slot b,
                                  float gap, float thickness, cui_color color,
                                  float pulse);

/* A ball-and-stick graph at a world point — the recurring neural-net motif —
 * in ONE call instead of many sphere+link calls. Places `nodes` balls
 * deterministically in a cloud of radius `spread` (seeded, so it does not
 * jitter between frames) and joins each to `sticks` neighbours. */
CUI_API void cui_iso_lattice(cui_iso_scene *s, float cx, float cy, float z,
                             int nodes, int sticks, float spread, float ball,
                             cui_color ball_color, cui_color stick_color,
                             uint32_t seed);

/* A stack of `count` flat disks — the coin / database drum motif. Each disk is
 * a short wide cylinder of height `disk_h`, separated by `gap`. */
CUI_API void cui_iso_disk_stack(cui_iso_scene *s, float cx, float cy, float z_bottom,
                                float radius, int count, float disk_h, float gap,
                                int sides, cui_color top, cui_color side);

/* A caption anchored at a world point, centred on it and depth-sorted like a
 * solid. Pass a colour with alpha 0 to use the scene's text colour. */
CUI_API void cui_iso_text(cui_iso_scene *s, float x, float y, float z,
                          const char *text, cui_color color);

/* A triangular prism — a pitched roof or a ramp. The ridge runs along the x
 * axis; the footprint is w (ridge length) by d, and the ridge rises `height`
 * above z_base. Both roof faces and both gable ends are shaded by the light. */
CUI_API void cui_iso_wedge(cui_iso_scene *s, float cx, float cy, float z_base,
                           float w, float d, float height, cui_color roof,
                           cui_color side);

/* CUI's reduced-motion path: when on, travelling pulses freeze at rest instead
 * of animating, so a motion-sensitive user still gets a complete scene. Call
 * once after creating the scene (wire it to CUI's own reduced-motion setting). */
CUI_API void cui_iso_set_reduced_motion(cui_iso_scene *s, bool on);

/* Place `count` solids evenly around a circle of `radius` at height z — the
 * hub-and-spoke layout. For each position it calls `place` with the slot index,
 * the world x/y and a 0..1 angle fraction; you draw the solid that belongs
 * there. When `conduit` is true an arc is drawn from the centre to each slot,
 * pulsing in sequence (t drives the animation, frozen under reduced motion). */
CUI_API void cui_iso_ring(cui_iso_scene *s, float cx, float cy, float z, float radius,
                          int count, float t, bool conduit, cui_color conduit_color,
                          void (*place)(cui_iso_scene *s, int index, float x, float y,
                                        float angle_frac, void *user),
                          void *user);

/* A tiled ground plane under the whole scene — the floor most dioramas sit on.
 * Drawn first, before all solids. (cx, cy) is the centre at height z; `cells`
 * divisions of `cell` world units each, so the plane spans cells*cell wide.
 * `line` colours the grid lines; a zero-alpha `fill` leaves the floor
 * transparent so your own background shows through. */
CUI_API void cui_iso_grid(cui_iso_scene *s, float cx, float cy, float z,
                          int cells, float cell, cui_color fill, cui_color line);

/* A flat ground ellipse (a contact shadow or ambient glow), drawn before all
 * solids. (cx, cy, z) is the world centre; rx/ry are screen-space radii already
 * scaled if you want, or pass world radii and let the camera scale them. */
CUI_API void cui_iso_shadow(cui_iso_scene *s, float cx, float cy, float z,
                            float rx, float ry, cui_color color);

/* Draw the whole batch back to front into a CUI context. Call this inside your
 * paint function. */
CUI_API void cui_iso_end(cui_iso_scene *s, cui_ctx *ui);

/* Nudge the solid added last toward (+) or away from (-) the camera when the
 * scene is sorted for drawing. The sort is by depth, which a diorama gets right;
 * a bias is for the tie the geometry cannot decide, such as a conduit that
 * should read in front of the platform it grazes. */
CUI_API void cui_iso_depth_bias_last(cui_iso_scene *sc, float bias);

/* ---- picking ------------------------------------------------------------- */

/* Click picking. Each returns true if the screen point (sx, sy) lands on that
 * solid: a box anywhere on its projected prism, a sphere by its screen circle, a
 * cylinder by its top ellipse plus the column beneath it. */
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
 * scene's draw order, or -1 when nothing is hit. Boxes and cylinders are tested
 * by footprint, spheres by their screen circle. Call it with the same camera
 * you drew with, between cui_iso_begin and cui_iso_end (it needs s->begun). */
CUI_API int cui_iso_pick(cui_iso_scene *s, float sx, float sy);

#ifdef __cplusplus
}
#endif

#endif /* CUI_ISO_H */