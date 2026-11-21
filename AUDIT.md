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
| 5 | High | `cui_iso_link_labeled` relabelled an unrelated solid when the scene was full: `link` silently dropped, then the tail solid got the label. | Read from code, `cui_iso.c:311-317` (Verified by inspection). | Fixed: guard on `count == before + 1` and the pushed index (see commit). |
| 6 | High | Source comments carried non-ASCII bytes (em dashes). MSVC without a BOM can warn or mangle them, and CUI builds with MSVC. | `LC_ALL=C grep -n '[^ -~\t]'` listed them (Verified). | Deferred: cosmetic, no behavioural effect with /utf-8. Tracked as a cleanup. |
| 7 | High | Cylinder face sort key ignored the camera azimuth, so strips ordered wrong off-axis. | `key[i] = cosf(am) + sinf(am)` at `cui_iso.c:599` (Verified by reading). | Deferred: the strip pass draws all strips into one silhouette, so ordering is invisible; the fix costs a branch per strip and buys nothing until glass is interleaved. |
| 8 | Medium | A ground grid and a contact shadow shared one kind, told apart by a `chamfer == 1.0f` sentinel. | `cui_iso.c:247`, `:525` (Verified). | Deferred: an encoding smell, not a defect. Belongs in a refactor with a proper `cui_iso_kind`. |
| 9 | Medium | Depth keys used magic offsets (`-1000`, `-2000`, `+ z*0.001`) to force draw order. | `cui_iso.c:225`, `:241`, `:267` (Verified). | Fixed for links (sorted by midpoint, real key). Shadows keep a constant because "behind everything" is their meaning; that constant is now documented, not magic. |
| 10 | Medium | No `NULL` tolerance: every constructor dereferenced `s` without a check. | Inferred from `cui_iso__push` and all constructors. | Fixed: `cui_iso__push` and the setters return early on `NULL`. |
| 11 | Medium | Light was fixed at construction; a caller could not set it, though it is a public struct. | `cui_iso_set_text_style` existed, no light equivalent lived in the header (Verified). | Fixed: `cui_iso_set_light`. |
| 12 | Medium | Constructors took up to 10 positional floats, so call sites were unreadable and transposable. `cui_iso_box(s, 0,0,30,70,70,60,10,top,side)` — which 60 is height? | Verified in `examples/top10.c` call sites; CUI's own convention is a designated options struct (`CUI__OPTS`). | Deferred to its own commit: `cui_iso_box_opts` + macro, keeping the positional form as a thin wrapper. |
| 13 | Low | Capacity documented as fixed but the test never exercised overflow. | Read from code, `CUI_ISO_MAX_SOLIDS` (Verified). | Fixed: boundary test pushes 4000 solids. |
| 14 | Low | Picking only had `hit_box`; spheres and cylinders were unpickable, so `cui_iso_pick` returned -1 on them. | `cui_iso.c` picking section (Verified). | Fixed: `hit_sphere`, `hit_cylinder`, and `pick` over all kinds. |
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
