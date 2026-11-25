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
| 5 | High | `cui_iso_link_labeled` hung its label on whatever solid happened to be last when the link itself was dropped (scene full, or not begun), so an unrelated solid got relabelled. | Read from code, `cui_iso.c:306-321` (Verified by inspection). Found by review, not by a test. | Fixed in `6db26d3`: the label is written only when this call pushed a solid (`count == before + 1`). An earlier revision of this table claimed it fixed before it was; that was wrong and is the reason the row is worded this way. Test: full scene plus a labelled link leaves the count unchanged. |
| 6 | High | Source comments carried non-ASCII bytes (em dashes). MSVC without a BOM can warn or mangle them, and CUI builds with MSVC. | `LC_ALL=C grep -n '[^ -~\t]'` listed them (Verified). | Deferred: cosmetic, no behavioural effect with /utf-8. Tracked as a cleanup. |
| 7 | High | Cylinder face sort key ignored the camera azimuth, so strips ordered wrong off-axis. | `key[i] = cosf(am) + sinf(am)` at `cui_iso.c:599` (Verified by reading). | Deferred: the strip pass draws all strips into one silhouette, so ordering is invisible; the fix costs a branch per strip and buys nothing until glass is interleaved. |
| 8 | Medium | A ground grid and a contact shadow shared one kind, told apart by a `chamfer == 1.0f` sentinel. | `cui_iso.c:247`, `:525` (Verified). | Deferred: an encoding smell, not a defect. Belongs in a refactor with a proper `cui_iso_kind`. |
| 9 | Medium | Depth keys used magic offsets (`-1000`, `-2000`, `+ z*0.001`) to force draw order. | `cui_iso.c:225`, `:241`, `:267` (Verified). | Fixed for links (sorted by midpoint, real key). Shadows keep a constant because "behind everything" is their meaning; that constant is now documented, not magic. |
| 10 | Medium | No `NULL` tolerance: `cui_iso__push` and the entry points dereferenced `s` without a check, so a mis-wired client crashed instead of drawing nothing. | Inferred from `cui_iso__push` and all constructors (Verified by reading, `cui_iso.c:180-188`). | Fixed in `6db26d3`. `cui_iso__push` takes only the scene and the kind, so no caller can form `&s->cam` before the NULL check (this started as push taking the coordinates; that leftover was removed in `6e79764` when the stored depth field went); `reset`, `begin`, `set_text_style` and `set_light` check too. Reproduced first as an ASan SEGV at `cui_iso.c:164` from `test_cui_iso_null`, then fixed. |
| 11 | Medium | Light was fixed at construction; a caller could not set it, though `cui_iso_light` is a public struct and the README implies you can change it. | `cui_iso_set_text_style` exists, no light equivalent in the header (Verified, grep `set_light` = 0). | Fixed: `cui_iso_set_light` implemented in `6db26d3` and declared in `05b2b24`. The declaration had been omitted, so a clean checkout of `6db26d3` failed to compile the test; `05b2b24` is that repair. |
| 12 | Medium | Constructors took up to 10 positional floats, so call sites were unreadable and transposable. `cui_iso_box(s, 0,0,30,70,70,60,10,top,side)` does not say which 70 is the height. | Verified in `examples/top10.c` and `cases.c` call sites. CUI's own convention is a designated options struct (`CUI__OPTS`, `cui_table_opts { int _; ... }`). | Fixed: `cui_iso_opts.h` gives every solid an opts struct and an `_ex` function (`cui_iso_box_opts`, `cui_iso_box_ex`), zero-default via a leading `int _`. The positional forms stay, so no caller broke. `test_cui_iso_opts` renders one scene through both forms and requires byte-identical buffers (Verified, ctest green). |
| 13 | Low | Capacity was documented as fixed but nothing asserted it: an overflow dropped solids silently. | Read from code; `cui_iso_scene_count` did not exist, so a test could not observe the count. | Fixed: `CUI_ISO_MAX_SOLIDS` published in the header, `cui_iso_scene_count()` added, and the limits test asserts 4000 added boxes clamp to exactly 1024 (`ok capacity: 4000 boxes clamped to 1024`, Verified). |
| 14 | Low | Picking did not match the paint at all, and most kinds were not pickable. `cui_iso_hit_box` tested only the lid, so 6466 of 11931 box pixels were dead to the pointer; the cylinder tested a flattened top ellipse, losing 2083 of 8291; spheres fell through `cui_iso_pick`. | `test_cui_iso_pick` counts painted vs hit per pixel (Verified: the 6466 and 2083 numbers are its own output on the pre-fix build). | Fixed: `cui_iso_hit_solid` is the convex hull of the box's eight projected corners, the cylinder hulls sampled rims, `cui_iso_hit_sphere` was added, and `cui_iso_pick` tests all three. Re-measured: box dead 252 / phantom 540, cylinder dead 216 / phantom 2, both inside allowance (Verified). |
| 15 | Low | `cui_iso_slab` reused `height` as its chamfer clamp, so a hull thinner than the chamfer changed shape silently. | `cui_iso.c:208-211` (Verified by reading). | Deferred: documented, not changed; changing it alters existing output. |

| 16 | Critical | The committed tree did not build: `test_cui_iso_null.c` called `cui_iso_set_light`, whose declaration was still uncommitted. | Exported `HEAD` with `git archive` to an empty directory and compiled the test there: `call to undeclared function 'cui_iso_set_light'` (Verified). The working tree passed only because it had the declaration. | Fixed in `05b2b24`. A clean export of `HEAD` now builds all sources and passes the suite (9 of 9 at `e294561`; it was 6 of 6 when this row was written). |
| 17 | Critical | `CMakeLists.txt` hardcoded `${CMAKE_CURRENT_SOURCE_DIR}/../vendor/CUI`, so the project built only inside this workspace. | Same export configured elsewhere: `add_subdirectory given source "/tmp/hc/../vendor/CUI" which is not an existing directory` (Verified). | Fixed: `CUI_ISO_CUI_DIR` cache variable, defaulting to that path, with a `FATAL_ERROR` that names the variable. Verified by building an export that contains no `vendor/` at all, pointed at CUI with `-D`. |

| 18 | Note | CUI's guide says size matters: check what a change adds to `examples/hello` in a `MinSizeRel` build. The library's cost was unmeasured until now. | `hello` built as is: **291648** bytes. A minimal CUI app that draws one rect: **274296**. The same app using cui-iso (box, slab, sphere, link): **291488**. So the layer's marginal cost is **17192 bytes, 16.8 KiB, about 6%** of `hello`, and an app that uses it is 160 bytes *smaller* than `hello` because dead stripping drops CUI paths it never calls (Verified, `ls -l` on three `-Os -DNDEBUG -Wl,-dead_strip` links). | Measured, above. Not a defect: recorded so a reviewer can weigh it without rebuilding. |
| 19 | Critical | A diorama built from theme tokens was invisible in the light theme. `CUI_COLOR_SURFACE` and `CUI_COLOR_SURFACE_RAISED` both resolve to 255,255,255 there, so a plate and the block standing on it were one colour. It looked right in dark, the only theme it had been rendered in. | Probed the resolved values in both themes: light surface 255,255,255 raised 255,255,255; dark surface 24,24,24 raised 31,31,31. Tone histogram of the render: light 93.9% light pixels, dark 97.3% dark, so the theme applies and the fault is the palette, not the call (Verified). | Fixed: `cui_iso_palette` is a ramp from the theme BACKGROUND to its FOREGROUND, opaque throughout. Measured light 249/234/217/224/202/11, dark 21/31/43/38/53/242, adjacent steps at least 7 apart. `test_cui_iso_palette` asserts it in both themes; against the old palette it reports 4 failures and exits 1. |
| 20 | Medium | `cui_mix` already exists in CUI (public, OKLab, premultiplied); a second sRGB blend had been written into `cui_iso.c`. Two blends for one job is the duplicate-helper decay CONTRIBUTING warns about. | Read `src/cui_theme.c:55` and `include/cui/cui.h` (Verified). The bespoke version also lerped RGB channels directly, which is perceptually worse. | Fixed: the palette calls `cui_mix`; the duplicate was deleted, not wrapped. |
| 21 | Low | `cui_iso_pick`'s header comment still said it needed `s->begun`, untrue since `6e79764`. | Read `cui_iso.h` against the body (Verified). | Fixed: the comment says it is safe after `cui_iso_end`, and why. |
| 22 | Medium | The positional constructors remain callable, so a call that names no field still compiles and still reads as `cui_iso_box(s, 0, 0, 30, 70, 70, 60, 10, top, side)`. | Measured: 66 positional calls against 91 named calls in example and test code; 0 positional solid constructors declared by `cui_iso.h` (named-only since `a0969a0`; `3f79075` briefly restored them and was reverted). Correction: the earlier claim that a public-header-only caller could not draw was wrong, because `_named` expands to `_ex`, which `cui_iso_opts.h` declares. Two positional declarations stay public on purpose: `cui_iso_text` (three floats and a string) and `cui_iso_ring` (count, time, bool, callback); and 4 predicates still declared public at 6 to 8 floats each, `cui_iso_hit_box` 7, `cui_iso_hit_solid` 8, `cui_iso_hit_sphere` 6, `cui_iso_hit_cylinder` 7. | Partly addressed, deliberately not closed. `cui_iso.h` now declares one `_opts` struct and one `_ex` function per solid plus the `_named` macros; every constructor has a named form (`test_cui_iso_named`), the three layouts were converted outright, and the examples went from 143 positional calls to 66, checked byte-identical by rendering dark and light before and after. Two things were left on purpose: the positional definitions survive as internal forwards because the forwarding tests and existing call sites use them, and the four hit-test predicates still take six to eight floats. |
| 23 | Note | Every public constructor now has a named-argument form: the fourteen solids through `*_ex`, and the three layouts through `cui_iso_layout_row_ex`, `cui_iso_layout_ring_ex`, `cui_iso_layout_grid_ex`. Where the solids take one opts pointer, a layout takes `(out, n, *opts)` because the count is not a tunable. `test_cui_iso_layout` asserts the named and positional layouts place every slot bit-identically, and application call sites use the named forms (no positional layout call remains outside the library's own forwards and that test). | Verified: ctest 8 of 8 under ASan and UBSan; grep over `examples/` and the tests for the positional names returns only the library and the equivalence test. | Fixed: named-argument forms exist for all seventeen constructors; applications use them. |

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
## Review pass over the pull request (11 findings)

Closed upstream-facing review findings, each with the check that proves it.

| # | Finding | Evidence after the fix |
|---|---------|------------------------|
| 1 | A parent `BUILD_SHARED_LIBS=ON` could turn the library into a shared object. | `add_library(cui_iso STATIC ...)`; configured with `-DBUILD_SHARED_LIBS=ON` and the target is still static. |
| 2 | `<cui/cui_iso_opts.h>` did not resolve in `add_subdirectory` builds. | `include/` added to `BUILD_INTERFACE`; a source-tree build includes it from `extensions/cui-iso/include/cui/`. |
| 3 | Sanitizers covered the test drivers only, so ASan saw no library code. | Directory-wide `add_compile_options`/`add_link_options`. Found by failure first: a sanitized library linked into an unsanitized executable failed with `undefined __asan_init`. |
| 4 | Windowed examples were configured even when `CUI_WITH_APP=OFF` removed the app layer. | Both `cui_app_target` uses are inside `if(CUI_WITH_APP)`. |
| 5 | Install shipped targets but no `find_package` config. | `cui_isoConfig.cmake` + version file installed under `lib/cmake/cui_iso`; a fresh project using `find_package(cui_iso CONFIG REQUIRED)` configures, builds and runs. |
| 6 | Cylinder strips sorted by a fixed world angle, so a tall cylinder's rear strips could paint over its visible barrel once the camera orbits. | Key is `cui_iso_depth(cam, ...)`. The sort test still renders byte-identically, which is the proof the order only changes off `az = 0`. |
| 7 | `cui_iso_pick` selected solids outside the bounds `cui_iso_end` clipped to. | Pick rejects the point, not the solid; a partly clipped solid stays selectable on its visible part. Test: two self-locating cases in `test_cui_iso_pick`. |
| 8 | Pick kept the first hit on a depth tie while the sort painted the later solid on top. | `>=` in the comparison; the test renders two spheres, picks, swaps the insertion order and requires the answer to change. |
| 9 | `extern "C"` closed before `cui_iso_palette_default`, so a C++ caller linked a mangled symbol. | Guard extended past it; `nm` on a C++ object shows `_cui_iso_palette_default`, not `__Z23cui_iso_palette_defaultP7cui_ctx`. |
| 10 | Recoverable UBSan findings printed and the tests still passed. | `-fno-sanitize-recover=all` in the sanitizer build. |
| 11 | The vendored `include/cui` headers drifted from the source headers, so an install shipped an old header. | CI diffs the two, with the em dash substituted. Caught only by testing against the install prefix rather than the source tree. |

## Review pass 2: CodeRabbit findings (39)

The automated reviewer returned 39 findings on the pull request. 34 were real
defects and were fixed in this pass; 5 were already closed by the header
rewrite that preceded it. Each row names the code that now carries the fix.

| # | Finding | Resolution |
|---|---------|------------|
| 1 | `cui_iso_hit_box` did not say it tests only the lid. | The public header says the box is tested on its projected LID and points to `cui_iso_hit_solid` for the full volume (`include/cui/cui_iso.h:259`). |
| 2 | The public examples used internal positional forms, so a public-header-only caller could not compile them. | The header example uses `cui_iso_box_named` and names `<cui/cui_iso_opts.h>`; the layout text points at the `_ex` forms (`include/cui/cui_iso.h:30`). |
| 3 | Captions were not documented as drawing above the solids. | Already closed: the header says a caption is anchored in world space but drawn in a final overlay pass (`include/cui/cui_iso.h:218`). |
| 4 | The `_named` macros depend on `CUI__OPTS`, which CUI defines only in C mode, so they were unusable from C++. | Already closed: the macro block sits under `#ifndef __cplusplus`; the `_ex` functions stay public for C++ clients (`include/cui/cui_iso_opts.h:214`). CI compiles both headers standalone in C and C++. |
| 5 | The named-constructor example did not compile. | Already closed: the example is `.cx/.w/.d/.height/.top/.side` only, no invalid field (`include/cui/cui_iso_opts.h:207`). |
| 6 | The cylinder side count was stored as a float and cast back at draw time, which is undefined past the int range. | The count is clamped to 8..64 at submission, before it is stored (`cui_iso.c:361`, `cui_iso.c:717`). |
| 7 | Picking sampled 20 rim points while the renderer used the clamped side count, so a vertex on the barrel was not clickable. | One sampler now serves both: `iso__cyl_rim()` builds the rim the way the renderer does, and `cui_iso_pick` reads the side count off the solid (`cui_iso.c:1233`). `cui_iso_hit_cylinder` has no side count, so it uses the roundest cylinder the renderer can draw (64) and says so. |
| 8 | A negative label value was still printed as a number. | The number is formatted only when `label_value >= 0` and `label_decimals >= 0`; a negative value keeps the label-only form, and zero still prints (`cui_iso.c:889`). |
| 9 | `cui_iso_hit_box` accepted any click on a zero-scale camera, because every cross product was zero. | A zero scale is rejected, like `cui_iso_hit_solid` (`cui_iso.c:1195`). |
| 10 | The wedge always drew the far-side `+x` gable, which painted over the near roof after a half-turn. | Each face is tested against the camera direction and its own normal; only camera-facing faces are drawn and the world-space lighting is unchanged (`cui_iso.c:726`). |
| 11 | `cui_iso_end` popped a clip it had never pushed when the clip stack was full, removing the caller's clip. | CUI grew `cui_push_clip_checked()`, which reports whether the push was taken (`src/cui_draw.c:63`). When the push is refused the scene draws nothing and pops nothing (`cui_iso.c:975`). |
| 12 | Reduced motion rewrote a disabled pulse (value 2) to 0.5, switching the pulse on. | Only a pulse already inside `[0,1]` is frozen at the midpoint (`cui_iso.c:964`). |
| 13 | The pick index contract did not say whether it was the draw order or the submission order. | Already closed: the header says the index is the order the solids were submitted in, so the answer maps back to the call that added the solid (`include/cui/cui_iso.h:276`). |
| 14 | `cui_iso_set_reduced_motion(NULL, true)` dereferenced NULL. | NULL guard added (`cui_iso.c:554`) and the entry point is in the NULL block of the null test. |
| 15 | The lattice left node 0 isolated and gave a two-node, one-stick lattice no edge at all. | Neighbours are chosen by geometric distance and each pair is linked once (`cui_iso.c:448`). The limits test asserts the solid counts: 2 nodes give 3 solids, 4 nodes 10, 24 nodes 300. |
| 16 | The lifetime of a label string was not documented. | Already closed: the header says the string is not copied and must stay valid until `cui_iso_end` returns (`include/cui/cui_iso.h:221`). |
| 17 | `cui_iso_pick` tested the un-chamfered box, so a cut-away corner was clickable. | `iso__hit_box_volume()` builds the hull from the same chamfered footprint the renderer uses (`cui_iso.c:1278`). `cui_iso_hit_solid` keeps its rectangular contract. The pick test asserts a click in the cut-away corner selects nothing. |
| 18 | `cui_iso_begin` with a NULL camera left the previous batch alive, so the next `cui_iso_end` drew the old scene. | A NULL camera clears the batch and marks the scene not-begun (`cui_iso.c:263`). |
| 19 | The disk stack looped over every disk even when the scene was full or absent. | The stack returns early outside a batch and is capped at the room left in the batch, so a million-disk stack stops after the disks that fit (`cui_iso.c:513`). The limits test asserts the batch ends exactly at the cap. |
| 20 | The grid cell count rode in a float and was cast back to int at draw time, which is undefined past the int range. | The count is validated and capped at submission (`cui_iso.c:340`), and the cap is small enough for the float round trip to be exact. |
| 21 | The cylinder cap passed a centre point and a repeated vertex, so CUI feathered the internal radial edges. | The cap is the rim polygon with `sides` vertices and no centre (`cui_iso.c:764`). |
| 22 | `cui_iso_layout_grid` returned a positive count for negative dimensions and could not represent a product past `INT_MAX`. | Both dimensions and the product are checked before anything is written; an invalid grid returns 0 (`cui_iso.c:134`). The limits test asserts 64x64 gives 4096 and that 4096x2, 2x2049, -1x4 and 0x0 are refused. |
| 23 | `cui_iso_depth_bias_last` changed what `cui_iso_pick` reported after the frame was painted. | The write is refused outside a batch (`cui_iso.c:1044`), so the pick always matches the frame. |
| 24 | `cui_iso_text_ex` and `cui_iso_text` disagreed about a NULL caption, so the two spellings left different scene counts. | The rule lives in one place: `cui_iso_text` refuses a NULL caption and `_ex` delegates to it (`cui_iso_opts.c:71`). |
| 25 | The default configure failed on a clean checkout because the CUI path did not exist. | `CUI_ISO_CUI_DIR` now starts empty and auto-detects the CUI checkout two levels up (or `../vendor/CUI`); the error names the variable. A configure with no `-D` was run and found the tree (`CMakeLists.txt:21`). |
| 26 | The null test checked the scene count after a rejected insertion, which cannot see a label written onto another solid. | The test now renders two frames that differ by one refused call and compares them byte for byte (`test_cui_iso_null.c:29`). With the label guard removed the frames differ and the test fails. |
| 27 | The demo shadow alpha went negative before the `uint8_t` cast, which is undefined behaviour. | The alpha is clamped in float first (`scene_demo.c:170`). |
| 28 | The top10 example compared only the alpha channel to decide which stage pulses, so every stage pulsed. | The stage index decides (`examples/top10.c:92`). |
| 29 | The build and the install each used a full copy of the public headers, so a fix had to be made twice. | `include/cui/` is canonical; `cui_iso.h` and `cui_iso_opts.h` are thin forwarders that keep the old include spellings working. |
| 30 | The sort test could not tell a correct sort from an inverted one. | The test keeps the insertion-order comparison and adds a depth-direction assertion: two overlapping boxes, and the nearer one must be the one painted on top, in either insertion order (`test_cui_iso_sort.c:74`). With the comparator inverted the test reports the far box's colour and fails. |
| 31 | The top10 example took an output directory and then wrote to a hard-coded path. | The directory is a file-scope variable and every case writes into it (`examples/top10.c`). |
| 32 | The top10 pulse phase could go negative, and a negative pulse is the "no pulse" sentinel. | The phase is wrapped into `[0,1)` (`examples/top10.c`). |
| 33 | `end_case` ignored the PNG write result and the allocation, and `main` returned 0 after a failed write. | `end_case` returns a bool and checks both, and `main` returns 1 when any case fails (`examples/top10.c:41`). |
| 34 | The top10 example built a shell command from `argv[1]`, so a path with a space or a metacharacter was run by the shell. | The directories are created with `mkdir(2)` (`_mkdir` on Win32) through a portable recursive helper; no shell is involved (`examples/top10.c:298`). |
| 35 | The two source slots were laid out along x, so they landed between the stations instead of in a column beside the first. | The sources differ along y (`examples/top10.c:124`). |
| 36 | The `text` case used a fixed near-white colour, unreadable on the light theme. | It uses the theme token `gpal.ink` (`cases.c:111`). |
| 37 | The pipeline example did not set a theme, so it started light while its labels were fixed near-white. | The example asks for the dark theme, which matches its fixed label colours (`examples/pipeline.c:141`). |
| 38 | The README said captions were depth-sorted like a solid. | The README says a caption is drawn on top of all solids in a final overlay pass (`README.md`). |
| 39 | The README called a positional `cui_iso_link_labeled` with `&scene`, which does not exist and would not compile. | The example calls `cui_iso_link_labeled_named(scene, ...)` with named fields (`README.md`). |

### What this pass does not claim

- The extension needs one new symbol from CUI, `cui_push_clip_checked`, and
  this pass adds it to CUI itself (`include/cui/cui.h`, `src/cui_draw.c`), so
  CUI and the extension ship together. A consumer who takes only
  `extensions/cui-iso` must take that CUI change too; the configure step
  checks for the symbol and stops with the instruction instead of failing at
  link time.
- The three new test assertions were checked against deliberately broken
  code: removing the label guard, inverting the depth comparator and ignoring
  the chamfer each make the matching test fail. They are not a proof that no
  other defect remains.
- The CI runners, the macOS backends and the Windows build were not exercised
  here. Only the software renderer under ASan and UBSan on this machine.
