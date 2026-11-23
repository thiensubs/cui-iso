/* test_cui_iso_palette.c - the theme ramp must survive both themes.
 *
 * The first palette used CUI_COLOR_SURFACE and CUI_COLOR_SURFACE_RAISED.
 * Measured, those both resolve to 255,255,255 in the light theme, so a plate
 * and the block standing on it were the same colour: the diorama vanished in
 * light while looking right in dark. A screenshot does not catch that
 * reliably, so the property is asserted here:
 *
 *   in each theme, every pair of surfaces that stack differs by at least GAP
 *   levels on some channel, and the ink is well separated from the ground.
 */
#include "cui_iso.h"
#include "cui_iso_opts.h"
#include <cui/cui_render.h>

#include <stdio.h>
#include <stdlib.h>

#define GAP 6

static int fails;

static int chdiff(unsigned char a, unsigned char b) {
    return a > b ? a - b : b - a;
}

static int parted(cui_color a, cui_color b) {
    return chdiff(a.r, b.r) >= GAP || chdiff(a.g, b.g) >= GAP ||
           chdiff(a.b, b.b) >= GAP;
}

static void must(int ok, const char *what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    } else {
        printf("ok   %s\n", what);
    }
}

static void audit(int light, const char *name) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw),
                                             .theme = light ? CUI_THEME_LIGHT : CUI_THEME_DARK });
    cui_iso_palette p = cui_iso_palette_default(ui);
    char msg[96];

    snprintf(msg, sizeof msg, "%s: plate stands off floor", name);
    must(parted(p.plate, p.floor), msg);
    snprintf(msg, sizeof msg, "%s: plate side reads darker than plate", name);
    must(parted(p.plate_side, p.plate), msg);
    snprintf(msg, sizeof msg, "%s: block stands off plate", name);
    must(parted(p.block, p.plate), msg);
    snprintf(msg, sizeof msg, "%s: block side reads off block", name);
    must(parted(p.block_side, p.block), msg);
    snprintf(msg, sizeof msg, "%s: back face differs from front face", name);
    must(parted(p.plate_side, p.block_side), msg);
    snprintf(msg, sizeof msg, "%s: ink is not the ground", name);
    must(parted(p.ink, p.floor), msg);

    cui_destroy(ui);
    cui_sw_destroy(sw);
}

int main(void) {
    audit(1, "light");
    audit(0, "dark");
    if (fails) {
        printf("%d palette check(s) failed\n", fails);
        return 1;
    }
    printf("all palette checks passed\n");
    return 0;
}
