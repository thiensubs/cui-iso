/* cui_iso_opts.h - the named-field forms of the cui-iso solids.
 *
 * One options struct and one function per solid, so a call site reads as named
 * fields and a diff can show which argument moved:
 *
 *   cui_iso_box_ex(sc, &(cui_iso_box_opts){ .cx = 0, .cy = 0, .z_top = 30,
 *                                            .w = 70, .d = 70, .height = 60,
 *                                            .top = a, .side = b });
 *
 * Same drawing path, same sort, no new state. `int _` comes first, so a zeroed
 * struct is a valid default, as in CUI components.
 *
 * This header is where the solids are documented: the scene, the camera and
 * the picking helpers live in <cui/cui_iso.h>.
 */
#ifndef CUI_ISO_OPTS_H
#define CUI_ISO_OPTS_H

#include "cui_iso.h"

/* A chamfered box: a rectangular prism with its vertical corners cut by
 * `chamfer` world units, which is what makes the silhouette read as 3D rather
 * than a flat hexagon. (cx, cy) is the footprint centre; the box sits with its
 * TOP at z = z_top and extends down `height`. `top` is the top-face colour;
 * `side` is the base side colour, shaded per face by the scene's light. */
typedef struct cui_iso_box_opts {
    int _;
    float cx, cy, z_top;
    float w, d, height, chamfer;
    cui_color top, side;
} cui_iso_box_opts;

/* A thin flat slab, convenience for a box of small height (a platform). */
typedef struct cui_iso_slab_opts {
    int _;
    float cx, cy, z_top;
    float w, d, height;
    cui_color top, side;
} cui_iso_slab_opts;

/* A translucent glass case over a footprint, with a neon frame on its top
 * edges. Drawn after opaque solids. `tint` colours the glass panels; the frame
 * uses `tint` at higher alpha. */
typedef struct cui_iso_glass_opts {
    int _;
    float cx, cy, z_bottom;
    float w, d, height;
    cui_color tint;
} cui_iso_glass_opts;

/* A vertical cylinder standing on z_bottom: pillars, tanks, columns, pipes.
 * `sides` is the tessellation (clamped to 8..64; 24 looks round at diorama
 * scale). The side is lit strip-by-strip like a box, so it shades consistently
 * with the boxes around it. */
typedef struct cui_iso_cylinder_opts {
    int _;
    float cx, cy, z_bottom;
    float radius, height;
    int sides;
    cui_color top, side;
} cui_iso_cylinder_opts;

/* A depth-sorted ball at a world point: molecule atoms, node markers,
 * particles. Drawn with a top-left highlight so it reads as a sphere. */
typedef struct cui_iso_sphere_opts {
    int _;
    float x, y, z, radius;
    cui_color color;
} cui_iso_sphere_opts;

/* A flat ground ellipse (a contact shadow or ambient glow), drawn before all
 * solids. (cx, cy, z) is the world centre; rx/ry are the radii. */
typedef struct cui_iso_shadow_opts {
    int _;
    float cx, cy, z;
    float rx, ry;
    cui_color color;
} cui_iso_shadow_opts;

/* A tiled ground plane under the whole scene -- the floor most dioramas sit on.
 * Drawn first, before all solids. (cx, cy) is the centre at height z; `cells`
 * divisions of `cell` world units each, so the plane spans cells*cell wide.
 * `cells` is clamped to CUI_ISO_MAX_GRID_DIV divisions per side.
 * `line` colours the grid lines; a zero-alpha `fill` leaves the floor
 * transparent so your own background shows through. */
typedef struct cui_iso_grid_opts {
    int _;
    float cx, cy, z;
    int cells;
    float cell;
    cui_color fill, line;
} cui_iso_grid_opts;

/* A connection between two world points -- the "sense flow" primitive: conduits,
 * pipes, edges in a topology or pipeline. Drawn as a soft glow + body + bright
 * core. `pulse` in 0..1 places a travelling dot at that fraction along the link
 * (pass a value outside 0..1 for no pulse), so a scene can animate flow by
 * moving one value each frame. Sorted at the link's midpoint, so it sits
 * between the solids it joins. */
typedef struct cui_iso_link_opts {
    int _;
    float x1, y1, z1, x2, y2, z2;
    float thickness, pulse;
    cui_color color;
} cui_iso_link_opts;

/* A link plus a label and a live number floating at the midpoint -- the
 * "LLM --3.2 tok/s--> Optimize" look from pipeline art. `decimals` is digits
 * after the point for `value` (pass 0 for "12", 1 for "3.2"); a negative value
 * draws the label with no number. The label uses the scene's text style.
 *
 * `label` is not copied: keep the string valid until cui_iso_end returns (or
 * until the batch is discarded), or the label reads freed memory. */
typedef struct cui_iso_link_labeled_opts {
    int _;
    float x1, y1, z1, x2, y2, z2;
    float thickness, pulse;
    cui_color color;
    const char *label;
    float value;
    int decimals;
} cui_iso_link_labeled_opts;

/* A curved conduit that bulges above the straight path -- the ring conduits and
 * curved flow arrows in pipeline art. `bulge` is how far (world units) the
 * midpoint lifts toward the camera and up; 0 collapses to a straight link.
 * Draws with the same glow/body/core layering as the link. */
typedef struct cui_iso_arc_opts {
    int _;
    float x1, y1, z1, x2, y2, z2;
    float bulge, thickness, pulse;
    cui_color color;
} cui_iso_arc_opts;

/* A triangular prism -- a pitched roof or a ramp. The ridge runs along the x
 * axis; the footprint is w (ridge length) by d, and the ridge rises `height`
 * above z_base. Both roof faces and both gable ends are shaded by the light. */
typedef struct cui_iso_wedge_opts {
    int _;
    float cx, cy, z_base;
    float w, d, height;
    cui_color roof, side;
} cui_iso_wedge_opts;

/* A caption anchored at a world point, centred on it. Captions are drawn in a
 * final overlay pass after every solid, so a solid never paints over a
 * caption. A zero-alpha colour uses the scene's text colour.
 *
 * `text` is not copied: keep the string valid until cui_iso_end returns (or
 * until the batch is discarded), or the caption reads freed memory. A NULL
 * text adds nothing to the scene. */
typedef struct cui_iso_text_opts {
    int _;
    float x, y, z;
    const char *text;
    cui_color color;
} cui_iso_text_opts;

/* A ball-and-stick graph at a world point -- the recurring neural-net motif --
 * in ONE call instead of many sphere+link calls. Places `nodes` balls
 * deterministically in a cloud of radius `spread` (seeded, so it does not
 * jitter between frames) and joins each to its `sticks` nearest neighbours. */
typedef struct cui_iso_lattice_opts {
    int _;
    float cx, cy, z;
    int nodes, sticks;
    float spread, ball;
    cui_color ball_color, stick_color;
    uint32_t seed;
} cui_iso_lattice_opts;

/* A stack of `count` flat disks -- the coin / database drum motif. Each disk is
 * a short wide cylinder of height `disk_h`, separated by `gap`. */
typedef struct cui_iso_disk_stack_opts {
    int _;
    float cx, cy, z_bottom;
    float radius, disk_h, gap;
    int count, sides;
    cui_color top, side;
} cui_iso_disk_stack_opts;

#ifdef __cplusplus
extern "C" {
#endif

CUI_ISO_API void cui_iso_box_ex(cui_iso_scene *s, const cui_iso_box_opts *o);
CUI_ISO_API void cui_iso_slab_ex(cui_iso_scene *s, const cui_iso_slab_opts *o);
CUI_ISO_API void cui_iso_glass_ex(cui_iso_scene *s, const cui_iso_glass_opts *o);
CUI_ISO_API void cui_iso_cylinder_ex(cui_iso_scene *s, const cui_iso_cylinder_opts *o);
CUI_ISO_API void cui_iso_sphere_ex(cui_iso_scene *s, const cui_iso_sphere_opts *o);
CUI_ISO_API void cui_iso_shadow_ex(cui_iso_scene *s, const cui_iso_shadow_opts *o);
CUI_ISO_API void cui_iso_grid_ex(cui_iso_scene *s, const cui_iso_grid_opts *o);
CUI_ISO_API void cui_iso_link_ex(cui_iso_scene *s, const cui_iso_link_opts *o);
CUI_ISO_API void cui_iso_link_labeled_ex(cui_iso_scene *s, const cui_iso_link_labeled_opts *o);
CUI_ISO_API void cui_iso_arc_ex(cui_iso_scene *s, const cui_iso_arc_opts *o);
CUI_ISO_API void cui_iso_wedge_ex(cui_iso_scene *s, const cui_iso_wedge_opts *o);
CUI_ISO_API void cui_iso_text_ex(cui_iso_scene *s, const cui_iso_text_opts *o);
CUI_ISO_API void cui_iso_lattice_ex(cui_iso_scene *s, const cui_iso_lattice_opts *o);
CUI_ISO_API void cui_iso_disk_stack_ex(cui_iso_scene *s, const cui_iso_disk_stack_opts *o);

/* ---- the spelling to use ------------------------------------------------
 *
 * Same idiom as cui_alert and cui_progress in CUI: the short name is a macro
 * over the _ex function and the options struct, so a call site reads as named
 * fields and a diff cannot swap two interchangeable numbers.
 *
 *     cui_iso_box_named(sc, .cx = 0, .w = 70, .d = 70, .height = 60,
 *                       .top = a, .side = b);
 *
 * The macro is C only: CUI__OPTS builds the options struct with a compound
 * literal, which C++ does not have. A C++ client calls the _ex function with a
 * struct it fills itself, which is the same thing without the macro.
 */
#ifndef __cplusplus
#define cui_iso_box_named(s, ...)         cui_iso_box_ex((s), CUI__OPTS(cui_iso_box_opts, __VA_ARGS__))
#define cui_iso_slab_named(s, ...)        cui_iso_slab_ex((s), CUI__OPTS(cui_iso_slab_opts, __VA_ARGS__))
#define cui_iso_glass_named(s, ...)       cui_iso_glass_ex((s), CUI__OPTS(cui_iso_glass_opts, __VA_ARGS__))
#define cui_iso_cylinder_named(s, ...)    cui_iso_cylinder_ex((s), CUI__OPTS(cui_iso_cylinder_opts, __VA_ARGS__))
#define cui_iso_sphere_named(s, ...)      cui_iso_sphere_ex((s), CUI__OPTS(cui_iso_sphere_opts, __VA_ARGS__))
#define cui_iso_shadow_named(s, ...)      cui_iso_shadow_ex((s), CUI__OPTS(cui_iso_shadow_opts, __VA_ARGS__))
#define cui_iso_grid_named(s, ...)        cui_iso_grid_ex((s), CUI__OPTS(cui_iso_grid_opts, __VA_ARGS__))
#define cui_iso_link_named(s, ...)        cui_iso_link_ex((s), CUI__OPTS(cui_iso_link_opts, __VA_ARGS__))
#define cui_iso_link_labeled_named(s, ...) cui_iso_link_labeled_ex((s), CUI__OPTS(cui_iso_link_labeled_opts, __VA_ARGS__))
#define cui_iso_arc_named(s, ...)         cui_iso_arc_ex((s), CUI__OPTS(cui_iso_arc_opts, __VA_ARGS__))
#define cui_iso_wedge_named(s, ...)       cui_iso_wedge_ex((s), CUI__OPTS(cui_iso_wedge_opts, __VA_ARGS__))
#define cui_iso_text_named(s, ...)        cui_iso_text_ex((s), CUI__OPTS(cui_iso_text_opts, __VA_ARGS__))
#define cui_iso_lattice_named(s, ...)     cui_iso_lattice_ex((s), CUI__OPTS(cui_iso_lattice_opts, __VA_ARGS__))
#define cui_iso_disk_stack_named(s, ...)  cui_iso_disk_stack_ex((s), CUI__OPTS(cui_iso_disk_stack_opts, __VA_ARGS__))
#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif

#endif /* CUI_ISO_OPTS_H */
