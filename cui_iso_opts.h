/* cui_iso_opts.h - named-field forms of the cui-iso solids.
 *
 * The positional constructors are terse but easy to transpose:
 * cui_iso_box(s, 0, 0, 30, 70, 70, 60, 10, top, side) does not say which 70 is
 * the height. These take the same arguments in a struct filled by field, the
 * way CUI fills its own options:
 *
 *   cui_iso_box_ex(sc, &(cui_iso_box_opts){ .cx = 0, .cy = 0, .z_top = 30,
 *                                         .w = 70, .d = 70, .height = 60,
 *                                         .top = a, .side = b });
 *
 * Same drawing path, same sort, no new state. The positional forms stay and are
 * unchanged, so existing code keeps building. `int _` comes first, so a zeroed
 * struct is a valid default, as in CUI components.
 */
#ifndef CUI_ISO_OPTS_H
#define CUI_ISO_OPTS_H

#include "cui_iso.h"

typedef struct cui_iso_box_opts {
    int _;
    float cx, cy, z_top;
    float w, d, height, chamfer;
    cui_color top, side;
} cui_iso_box_opts;

typedef struct cui_iso_slab_opts {
    int _;
    float cx, cy, z_top;
    float w, d, height;
    cui_color top, side;
} cui_iso_slab_opts;

typedef struct cui_iso_glass_opts {
    int _;
    float cx, cy, z_bottom;
    float w, d, height;
    cui_color tint;
} cui_iso_glass_opts;

typedef struct cui_iso_cylinder_opts {
    int _;
    float cx, cy, z_bottom;
    float radius, height;
    int sides;
    cui_color top, side;
} cui_iso_cylinder_opts;

typedef struct cui_iso_sphere_opts {
    int _;
    float x, y, z, radius;
    cui_color color;
} cui_iso_sphere_opts;

typedef struct cui_iso_shadow_opts {
    int _;
    float cx, cy, z;
    float rx, ry;
    cui_color color;
} cui_iso_shadow_opts;

typedef struct cui_iso_grid_opts {
    int _;
    float cx, cy, z;
    int cells;
    float cell;
    cui_color fill, line;
} cui_iso_grid_opts;

typedef struct cui_iso_link_opts {
    int _;
    float x1, y1, z1, x2, y2, z2;
    float thickness, pulse;
    cui_color color;
} cui_iso_link_opts;

typedef struct cui_iso_link_labeled_opts {
    int _;
    float x1, y1, z1, x2, y2, z2;
    float thickness, pulse;
    cui_color color;
    const char *label;
    float value;
    int decimals;
} cui_iso_link_labeled_opts;
typedef struct cui_iso_arc_opts {
    int _;
    float x1, y1, z1, x2, y2, z2;
    float bulge, thickness, pulse;
    cui_color color;
} cui_iso_arc_opts;

typedef struct cui_iso_wedge_opts {
    int _;
    float cx, cy, z_base;
    float w, d, height;
    cui_color roof, side;
} cui_iso_wedge_opts;

typedef struct cui_iso_text_opts {
    int _;
    float x, y, z;
    const char *text;
    cui_color color;
} cui_iso_text_opts;

typedef struct cui_iso_lattice_opts {
    int _;
    float cx, cy, z;
    int nodes, sticks;
    float spread, ball;
    cui_color ball_color, stick_color;
    uint32_t seed;
} cui_iso_lattice_opts;

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

CUI_API void cui_iso_box_ex(cui_iso_scene *s, const cui_iso_box_opts *o);
CUI_API void cui_iso_slab_ex(cui_iso_scene *s, const cui_iso_slab_opts *o);
CUI_API void cui_iso_glass_ex(cui_iso_scene *s, const cui_iso_glass_opts *o);
CUI_API void cui_iso_cylinder_ex(cui_iso_scene *s, const cui_iso_cylinder_opts *o);
CUI_API void cui_iso_sphere_ex(cui_iso_scene *s, const cui_iso_sphere_opts *o);
CUI_API void cui_iso_shadow_ex(cui_iso_scene *s, const cui_iso_shadow_opts *o);
CUI_API void cui_iso_grid_ex(cui_iso_scene *s, const cui_iso_grid_opts *o);
CUI_API void cui_iso_link_ex(cui_iso_scene *s, const cui_iso_link_opts *o);
CUI_API void cui_iso_link_labeled_ex(cui_iso_scene *s, const cui_iso_link_labeled_opts *o);
CUI_API void cui_iso_arc_ex(cui_iso_scene *s, const cui_iso_arc_opts *o);
CUI_API void cui_iso_wedge_ex(cui_iso_scene *s, const cui_iso_wedge_opts *o);
CUI_API void cui_iso_text_ex(cui_iso_scene *s, const cui_iso_text_opts *o);
CUI_API void cui_iso_lattice_ex(cui_iso_scene *s, const cui_iso_lattice_opts *o);
CUI_API void cui_iso_disk_stack_ex(cui_iso_scene *s, const cui_iso_disk_stack_opts *o);

#ifdef __cplusplus
}
#endif

#endif /* CUI_ISO_OPTS_H */
