/* cui_iso_internal.h - the positional constructors, for this library only.
 *
 * These are the original terse spellings. The public header no longer declares
 * them: cui_iso.h offers one options struct and one _ex function per solid, plus
 * the _named macros that name every field, so a call site can be read and a diff
 * can show which argument moved.
 *
 * They stay here because the _ex functions forward to them, and because the two
 * tests that check forwarding (opts, pick) need to call both spellings. Nothing
 * outside this directory has a caller of these, so they are internal, not
 * deprecated: this header is included by the .c files and by those tests only.
 */
#ifndef CUI_ISO_INTERNAL_H
#define CUI_ISO_INTERNAL_H

#include "cui_iso.h"

#ifdef __cplusplus
extern "C" {
#endif
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
/* A triangular prism — a pitched roof or a ramp. The ridge runs along the x
 * axis; the footprint is w (ridge length) by d, and the ridge rises `height`
 * above z_base. Both roof faces and both gable ends are shaded by the light. */
CUI_API void cui_iso_wedge(cui_iso_scene *s, float cx, float cy, float z_base,
                           float w, float d, float height, cui_color roof,
                           cui_color side);
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

#ifdef __cplusplus
}
#endif

#endif /* CUI_ISO_INTERNAL_H */
