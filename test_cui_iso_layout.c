/* test_cui_iso_layout.c - the named-argument layouts must place slots exactly
 * where the positional ones do, or a scene written one way and reviewed the
 * other would not agree. */
#include "cui_iso.h"
#include "cui_iso_internal.h"
#include "cui_iso_opts.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void same(const char *what, const cui_iso_slot *a, const cui_iso_slot *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z) {
            printf("FAIL %s slot %d: %g,%g,%g vs %g,%g,%g\n", what, i,
                   (double)a[i].x, (double)a[i].y, (double)a[i].z,
                   (double)b[i].x, (double)b[i].y, (double)b[i].z);
            fails++;
            return;
        }
    }
    printf("ok   %s (%d slots)\n", what, n);
}

int main(void) {
    cui_iso_slot p[13], q[13];

    int np = cui_iso_layout_row(p, 6, 10, 20, 30, 44);
    int nq = cui_iso_layout_row_ex(q, 6, &(cui_iso_layout_row_opts){ .x = 10, .y = 20, .z = 30, .spacing = 44 });
    if (np != nq) { printf("FAIL row count %d vs %d\n", np, nq); fails++; }
    else same("row", p, q, np);

    np = cui_iso_layout_ring(p, 5, 0, 0, 40, 90);
    nq = cui_iso_layout_ring_ex(q, 5, &(cui_iso_layout_ring_opts){ .x = 0, .y = 0, .z = 40, .radius = 90 });
    if (np != nq) { printf("FAIL ring count %d vs %d\n", np, nq); fails++; }
    else same("ring", p, q, np);

    np = cui_iso_layout_grid(p, 3, 2, 5, 6, 7, 20);
    nq = cui_iso_layout_grid_ex(q, 3, 2, &(cui_iso_layout_grid_opts){ .x = 5, .y = 6, .z = 7, .cell = 20 });
    if (np != nq) { printf("FAIL grid count %d vs %d\n", np, nq); fails++; }
    else same("grid", p, q, np);

    return fails ? 1 : 0;
}
