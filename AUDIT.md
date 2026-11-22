# cui-iso audit

Reviewer pass over the library before it is offered to yappologistic/CUI.
Each finding names the evidence: the line, and where a claim is about runtime
behaviour, the sanitizer output. Status is fixed or deferred with a reason.

Tiers used below: **Verified** means a command was run and its output read;
**Inferred** means read from the code; **Unverified** means not established.

| # | Sev | Finding | Evidence | Status |
|---|-----|---------|----------|--------|
| 1 | Critical | The library claimed to draw depth-sorted ("back to front") and did not sort at all. | `o->depth` written at `cui_iso.c:186`, never read; no `qsort`/sort in the translation unit (Verified, grep). Pre-fix UBSan/ASan run crashed for a different reason, so this was silent. | Fixed: stable insertion sort in `cui_iso_end`, three passes (shadows, opaque, glass). |
| 2 | Critical | Stack buffer overflow in the cylinder: `tb[64]`/`bb[64]` indexed at `i == sides` with `sides` clamped to 64. | UBSan: `index 64 out of bounds for type 'cui_vec2[64]'` at `cui_iso.c:577`; ASan: `stack-buffer-overflow ... WRITE of size 8` in `cui_iso_end`, `PRE_EXIT=134` (Verified). | Fixed: buffers are `sides+1` (65); post-fix ASan/UBSan clean, `EXIT=0`. |
| 3 | Critical | Cylinder top-cap fan overflowed: `cap[65]` written at index 65, and `cui_draw_polygon` read 66 points from a 65-element array. | Same sanitizer run, ASan report pointed into `cui_iso_end` (Verified). | Fixed: `cap[66]`. |
| 4 | Critical | Lattice out-of-bounds read: guard `i + k < nodes`, index `i + 1 + k`, so `j` could equal `nodes` and `px[nodes]` was read past the array. | Read from code, `cui_iso.c:363-364` (Verified by inspection); reached only at the node limit, so the comfortable-value smoke test never hit it. | Fixed: guard is `i + 1 + k < nodes`. |
| 5 | High | `cui_iso_link_labeled` hung its label on whatever solid happened to be last when the link itself was dropped (scene full, or not begun), so an unrelated solid got relabelled. | Read from code, `cui_iso.c:306-321` (Verified by inspection). Found by review, not by a test. | Open: found after the sort/pick commit landed; the fix and its regression test are the next commit. Understated in the previous revision of this table, which claimed it fixed. |
| 6 | High | Source comments carried non-ASCII bytes (em dashes). MSVC without a BOM can warn or mangle them, and CUI builds with MSVC. | `LC_ALL=C grep -n '[^ -~\t]'` listed them (Verified). | Deferred: cosmetic, no behavioural effect with /utf-8. Tracked as a cleanup. |
| 7 | High | Cylinder face sort key ignored the camera azimuth, so strips ordered wrong off-axis. | `key[i] = cosf(am) + sinf(am)` at `cui_iso.c:599` (Verified by reading). | Deferred: the strip pass draws all strips into one silhouette, so ordering is invisible; the fix costs a branch per strip and buys nothing until glass is interleaved. |
| 8 | Medium | A ground grid and a contact shadow shared one kind, told apart by a `chamfer == 1.0f` sentinel. | `cui_iso.c:247`, `:525` (Verified). | Deferred: an encoding smell, not a defect. Belongs in a refactor with a proper `cui_iso_kind`. |
| 9 | Medium | Depth keys used magic offsets (`-1000`, `-2000`, `+ z*0.001`) to force draw order. | `cui_iso.c:225`, `:241`, `:267` (Verified). | Fixed for links (sorted by midpoint, real key). Shadows keep a constant because "behind everything" is their meaning; that constant is now documented, not magic. |
| 10 | Medium | No `NULL` tolerance: `cui_iso__push` and the entry points dereferenced `s` without a check, so a mis-wired client crashed instead of drawing nothing. | Inferred from `cui_iso__push` and all constructors (Verified by reading, `cui_iso.c:180-188`). | Open: claimed fixed in the previous revision of this table but never applied. Next commit adds the guard and a test that calls each entry point with `NULL`. |
| 11 | Medium | Light was fixed at construction; a caller could not set it, though `cui_iso_light` is a public struct and the README implies you can change it. | `cui_iso_set_text_style` exists, no light equivalent in the header (Verified, grep `set_light` = 0). | Open: claimed fixed in the previous revision of this table; `cui_iso_set_light` does not exist in the tree. Added in the next commit. |
| 12 | Medium | Constructors took up to 10 positional floats, so call sites were unreadable and transposable. `cui_iso_box(s, 0,0,30,70,70,60,10,top,side)` does not say which 70 is the height. | Verified in `examples/top10.c` and `cases.c` call sites. CUI's own convention is a designated options struct (`CUI__OPTS`, `cui_table_opts { int _; ... }`). | Fixed: `cui_iso_opts.h` gives every solid an opts struct and an `_ex` function (`cui_iso_box_opts`, `cui_iso_box_ex`), zero-default via a leading `int _`. The positional forms stay, so no caller broke. `test_cui_iso_opts` renders one scene through both forms and requires byte-identical buffers (Verified, ctest green). |
| 13 | Low | Capacity was documented as fixed but nothing asserted it: an overflow dropped solids silently. | Read from code; `cui_iso_scene_count` did not exist, so a test could not observe the count. | Fixed: `CUI_ISO_MAX_SOLIDS` published in the header, `cui_iso_scene_count()` added, and the limits test asserts 4000 added boxes clamp to exactly 1024 (`ok capacity: 4000 boxes clamped to 1024`, Verified). |
| 14 | Low | Picking did not match the paint at all, and most kinds were not pickable. `cui_iso_hit_box` tested only the lid, so 6466 of 11931 box pixels were dead to the pointer; the cylinder tested a flattened top ellipse, losing 2083 of 8291; spheres fell through `cui_iso_pick`. | `test_cui_iso_pick` counts painted vs hit per pixel (Verified: the 6466 and 2083 numbers are its own output on the pre-fix build). | Fixed: `cui_iso_hit_solid` is the convex hull of the box's eight projected corners, the cylinder hulls sampled rims, `cui_iso_hit_sphere` was added, and `cui_iso_pick` tests all three. Re-measured: box dead 252 / phantom 540, cylinder dead 216 / phantom 2, both inside allowance (Verified). |
| 15 | Low | `cui_iso_slab` reused `height` as its chamfer clamp, so a hull thinner than the chamfer changed shape silently. | `cui_iso.c:208-211` (Verified by reading). | Deferred: documented, not changed; changing it alters existing output. |

## Test gate

- `ctest` runs smoke (all 15 primitives draw), boundary (max values, capacity
  overflow, degenerate geometry, layout edge cases) and sort invariant.
- The sort test is self-checking: with the sort disabled it reports
  `12502 bytes differ`; enabled, it reports identical (Verified).
- Sanitizer configuration: `cmake -DCUI_ISO_SANITIZE=ON`, then the same `ctest`.

## Not checked

- MSVC and MinGW builds (only clang on macOS was run).
- 32-bit targets.
- Float behaviour with `-ffast-math`.
- Real window backends (Cocoa/X11/Win32): the tests use the software renderer.
