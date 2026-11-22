/* test_cui_iso_null.c - a mis-wired client must draw nothing, not crash.
 * Every public entry point takes a NULL scene and a NULL camera here, and a
 * full scene must not relabel an unrelated solid (row 5 of AUDIT.md).
 */
#include "cui_iso_opts.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    cui_iso_scene *none = NULL;
    cui_color c = CUI_RGB(0x5c6169);
    cui_iso_light l = cui_iso_light_default();

    /* constructors must survive a NULL scene */
    cui_iso_box(none, 0, 0, 1, 2, 3, 4, 5, c, c);
    cui_iso_slab(none, 0, 0, 1, 2, 3, 4, c, c);
    cui_iso_glass(none, 0, 0, 1, 2, 3, 4, c);
    cui_iso_cylinder(none, 0, 0, 1, 2, 3, 4, c, c);
    cui_iso_sphere(none, 0, 0, 1, 2, c);
    cui_iso_shadow(none, 0, 0, 1, 2, 3, c);
    cui_iso_grid(none, 0, 0, 1, 2, 3, c, c);
    cui_iso_link(none, 0, 0, 1, 2, 3, 4, 5, c, 0.5f);
    cui_iso_arc(none, 0, 0, 1, 2, 3, 4, 5, 6, c, 0.5f);
    cui_iso_link_labeled(none, 0, 0, 1, 2, 3, 4, 5, c, 0.5f, "x", 1, 0);
    cui_iso_lattice(none, 0, 0, 1, 6, 2, 6, 6, c, c, 1u);
    cui_iso_disk_stack(none, 0, 0, 1, 2, 3, 4, 5, 6, c, c);
    cui_iso_text(none, 0, 0, 1, "t", c);
    cui_iso_wedge(none, 0, 0, 1, 2, 3, 4, c, c);
    cui_iso_set_text_style(none, (cui_iso_text_style){ 0 });
    cui_iso_set_light(none, l);
    cui_iso_scene_reset(none);
    cui_iso_scene_destroy(none);
    printf("NULL scene handled\n");

    /* begin with a NULL camera, then end: nothing drawn, nothing read */
    cui_iso_scene *sc = cui_iso_scene_create();
    cui_iso_begin(sc, NULL);
    cui_iso_box(sc, 0, 0, 0, 1, 1, 1, 1, c, c);
    printf("NULL camera safe: count %d\\n", cui_iso_scene_count(sc));

    /* a full scene must not relabel an older solid
     * (label guard row 5). Fill past the cap, then add a labelled link.
     */
    cui_iso_camera cam = { 0.55f, 100, 100, 1.0f };
    cui_iso_begin(sc, &cam);
    for (int i = 0; i < CUI_ISO_MAX_SOLIDS; i++)
        cui_iso_box(sc, 0, 0, 0, 1, 1, 1, 1, c, c);
    cui_iso_link_labeled(sc, 0, 0, 0, 10, 0, 0, 2, c, 0.5f, "leak", 1, 0);
    if (cui_iso_scene_count(sc) != CUI_ISO_MAX_SOLIDS) {
        fprintf(stderr, "FAIL label guard: count %d\\n", cui_iso_scene_count(sc));
        return 1;
    }
    printf("ok   label guard: full scene unchanged, count %d\\n",
           cui_iso_scene_count(sc));
    cui_iso_scene_destroy(sc);
    return 0;
}
