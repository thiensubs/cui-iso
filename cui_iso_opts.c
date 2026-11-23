/* cui_iso_opts.c - the named-field forms. Each one forwards to the positional
 * constructor, so there is exactly one drawing path and one set of defaults.
 */
#include "cui_iso_opts.h"
#include "cui_iso_internal.h"

void cui_iso_box_ex(cui_iso_scene *s, const cui_iso_box_opts *o) {
    if (!o) return;
    cui_iso_box(s, o->cx, o->cy, o->z_top, o->w, o->d, o->height, o->chamfer,
                o->top, o->side);
}

void cui_iso_slab_ex(cui_iso_scene *s, const cui_iso_slab_opts *o) {
    if (!o) return;
    cui_iso_slab(s, o->cx, o->cy, o->z_top, o->w, o->d, o->height,
                 o->top, o->side);
}

void cui_iso_glass_ex(cui_iso_scene *s, const cui_iso_glass_opts *o) {
    if (!o) return;
    cui_iso_glass(s, o->cx, o->cy, o->z_bottom, o->w, o->d, o->height, o->tint);
}

void cui_iso_cylinder_ex(cui_iso_scene *s, const cui_iso_cylinder_opts *o) {
    if (!o) return;
    cui_iso_cylinder(s, o->cx, o->cy, o->z_bottom, o->radius, o->height,
                     o->sides, o->top, o->side);
}

void cui_iso_sphere_ex(cui_iso_scene *s, const cui_iso_sphere_opts *o) {
    if (!o) return;
    cui_iso_sphere(s, o->x, o->y, o->z, o->radius, o->color);
}

void cui_iso_shadow_ex(cui_iso_scene *s, const cui_iso_shadow_opts *o) {
    if (!o) return;
    cui_iso_shadow(s, o->cx, o->cy, o->z, o->rx, o->ry, o->color);
}

void cui_iso_grid_ex(cui_iso_scene *s, const cui_iso_grid_opts *o) {
    if (!o) return;
    cui_iso_grid(s, o->cx, o->cy, o->z, o->cells, o->cell, o->fill, o->line);
}

void cui_iso_link_ex(cui_iso_scene *s, const cui_iso_link_opts *o) {
    if (!o) return;
    cui_iso_link(s, o->x1, o->y1, o->z1, o->x2, o->y2, o->z2,
                 o->thickness, o->color, o->pulse);
}

void cui_iso_link_labeled_ex(cui_iso_scene *s,
                             const cui_iso_link_labeled_opts *o) {
    if (!o) return;
    cui_iso_link_labeled(s, o->x1, o->y1, o->z1, o->x2, o->y2, o->z2,
                        o->thickness, o->color, o->pulse,
                        o->label, o->value, o->decimals);
}

void cui_iso_arc_ex(cui_iso_scene *s, const cui_iso_arc_opts *o) {
    if (!o) return;
    cui_iso_arc(s, o->x1, o->y1, o->z1, o->x2, o->y2, o->z2,
                o->bulge, o->thickness, o->color, o->pulse);
}

void cui_iso_wedge_ex(cui_iso_scene *s, const cui_iso_wedge_opts *o) {
    if (!o) return;
    cui_iso_wedge(s, o->cx, o->cy, o->z_base, o->w, o->d, o->height,
                  o->roof, o->side);
}

void cui_iso_text_ex(cui_iso_scene *s, const cui_iso_text_opts *o) {
    if (!o || !o->text) return;
    cui_iso_text(s, o->x, o->y, o->z, o->text, o->color);
}

void cui_iso_lattice_ex(cui_iso_scene *s, const cui_iso_lattice_opts *o) {
    if (!o) return;
    cui_iso_lattice(s, o->cx, o->cy, o->z, o->nodes, o->sticks, o->spread,
                    o->ball, o->ball_color, o->stick_color, o->seed);
}

void cui_iso_disk_stack_ex(cui_iso_scene *s,
                           const cui_iso_disk_stack_opts *o) {
    if (!o) return;
    cui_iso_disk_stack(s, o->cx, o->cy, o->z_bottom, o->radius, o->count,
                       o->disk_h, o->gap, o->sides, o->top, o->side);
}
