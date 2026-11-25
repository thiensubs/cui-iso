# Add an isometric diorama layer (cui-iso)

## What and why

CUI draws flat 2D primitives. Anything that should read as a small 3D scene has
to be hand-written per app: a projection, a face order, a per-face light, and a
painter's-algorithm sort. `mcpd-ui/mcpd_ui.c` in this tree does exactly that,
by hand, in about 300 lines.

This adds that code once, as an extension in its own directory, drawing only
through the existing `cui_draw_*` calls. No new dependencies, no second
renderer, works on every backend including software and WASM.

It is not a 3D engine. No textures, no perspective, no meshes; the sort is
painter's algorithm over whole solids. It makes crisp vector dioramas for
control surfaces and status scenes.

## What it contains

- Camera and projection: `cui_iso_camera`, `cui_iso_project`, `cui_iso_depth`.
- Solids: box, slab, wedge, cylinder, sphere, glass, grid, shadow, link,
  arc, lattice, disk stack, text.
- Slots: `cui_iso_layout_row`, `_ring`, `_grid`, `cui_iso_on`, and
  `cui_iso_link_between`, so a scene needs no hand-typed coordinates.
- Picking: a hit test per kind, and `cui_iso_pick`, safe after `cui_iso_end`.
- A theme-aware palette: `cui_iso_palette_default(ui)`.
- Optional clipping: `cui_iso_set_bounds`. Reduced motion:
  `cui_iso_set_reduced_motion`.

## What this PR changes in CUI itself

Two files, 35 lines added, nothing removed or changed in behaviour:

- `include/cui/cui.h` declares `cui_push_clip_checked` and `cui_current_clip`,
  and two capability macros, `CUI_HAS_PUSH_CLIP_CHECKED` and
  `CUI_HAS_CURRENT_CLIP`, so a consumer can tell whether the CUI it found has
  them: the version number does not say, because a patch release can add one.
- `src/cui_draw.c` implements them: the existing `cui_push_clip` becomes a
  forwarding wrapper around `cui_push_clip_checked`, and `cui_current_clip`
  reports the clip that is really in effect -- the intersection of every clip
  on the stack.

`cui_push_clip` refuses silently when the clip stack is 64 deep, and
`cui_pop_clip` pops anyway, so a caller that pushes a clip that was refused
takes the caller's own clip on the way out. The diorama needs to know whether
its push was taken: `cui_push_clip_checked` reports it. Every existing caller
keeps working, because `cui_push_clip` still behaves exactly as before.

`cui_current_clip` is the other half: a consumer that has to agree with what
CUI painted -- hit testing, for one -- needs the clip that decides, and the
clip that decides is the intersection of every clip on the stack. A clip the
consumer never pushed is invisible from the outside, so it cannot be worked
out from there.

The extension checks for both symbols at configure time, so a CUI without them
stops with one sentence instead of a link error, and the installed package
refuses such a CUI as well.

## Disclosure

    Author:     AI agent
    AI model:   Kimi K2 (kimi-k2-0905-preview) via NVIDIA NIM (first round);
                MiniMax-M3.1 (review pass 2)
    AI tool:    Kimi Code CLI, under human direction
    AI role:    analyzed the existing mcpd control surface, extracted the
                kernel, wrote the code, the docs and this report, and reviewed
                its own work against docs/CONTRIBUTING.md
    Human review: partial. The human operator directed scope, required the
                size measurement against examples/hello, rejected a first
                palette as invisible in the light theme, and read the audit
                table. No human has built or run this on their own machine.

## How it was tested

Run on macOS, AppleClang 17.0.0, x86_64, from a clean `git archive` export
(not from the working tree):

    cmake -S . -B b -DCMAKE_BUILD_TYPE=Release -DCUI_ISO_SANITIZE=ON \
          -DCMAKE_C_COMPILER=clang -DCUI_ISO_CUI_DIR=<a CUI checkout>
    cmake --build b -j
    ctest --test-dir b --output-on-failure

    CFG=0
    BUILD=0
    warnings=0
    100% tests passed out of 10

Two further checks, both mechanical rather than visual:

- Every example compiles against the public headers alone with
  `-Wall -Wextra -Werror`; none of them includes `cui_iso_internal.h`.
- `cmake --install` to a prefix, then a translation unit that includes only
  `<cui/cui_iso.h>` and `<cui/cui_iso_opts.h>` compiles against that prefix
  with `-Wall -Wextra -Werror`. Before the install rules, the prefix
  contained no headers at all.

The ten tests, and the negative control each was measured against:

| Test | Asserts | Negative control |
|---|---|---|
| `cui_iso_smoke` | every primitive draws and changes pixels | - |
| `cui_iso_limits` | max tessellation, capacity, degenerate geometry | before the fix ASan reported `stack-buffer-overflow`, exit 134 |
| `cui_iso_sort` | insertion order cannot change pixels, and the nearer box is painted on top | sort disabled: 12502 bytes differ; comparator inverted: the far box's colour wins |
| `cui_iso_pick` | painted pixels and hit-tested pixels agree, and the chamfered corner is not pickable | before: 6466 dead of 11931 (box), 2083 of 8291 (cylinder) |
| `cui_iso_opts` | named-field forms match positional, byte for byte | - |
| `cui_iso_null` | a NULL scene draws nothing, and a refused insertion changes nothing | before: ASan SEGV at `cui_iso.c:164`; label guard removed: the two frames differ |
| `cui_iso_palette` | stacked faces stay distinct in light and in dark | old palette: 4 failures |
| `cui_iso_layout` | named and positional layouts agree, bit for bit | - |
| `cui_iso_named` | all fourteen solids draw from named fields through their macro | stops compiling if a struct field is renamed or missed |

Also compiled with the exact command CI uses for the single-file
distribution (`-Werror`, `tests/windows_sdk_words.h` force-included): no
errors, no warnings.

Size, against this project's own rule, on the same machine with
`-Os -DNDEBUG -Wl,-dead_strip`, re-measured on this tree:

    examples/hello as is                    309904
    minimal CUI app (window, no widgets)    300696
    minimal app + one cui-iso diorama      317848

Marginal cost 17152 bytes (16.8 KiB, about 5.5% of hello). The old numbers in
the first version of this report were taken before the review pass; the
library grew, and the table above is the current tree.

Screenshots, light and dark: `docs/images/dioramas-light.png` and
`dioramas-dark.png`. Numeric check, because the author cannot view images: page
corners are 255,255,255 and 18,18,18, and 93.0% / 97.3% of sampled pixels fall on
those sides. A reviewer should look anyway.

## Review pass 2: the 39 CodeRabbit findings

The automated reviewer returned 39 findings on the pull request. 34 were real
defects and were fixed; 5 were already closed by the header rewrite that
preceded the pass. Every finding, the fix and the file that carries it are in
`extensions/cui-iso/AUDIT.md`, one row each.

What was done with them:

- The fixes were checked against deliberately broken code. Removing the label
  guard, inverting the depth comparator and ignoring the chamfer each make
  the matching test fail, so the new assertions are not vacuous.
- The ten tests were rebuilt under ASan and UBSan with no recovery
  (`-fno-sanitize-recover=all`), so a UBSan report fails the run instead of
  printing and passing.
- The public headers were made canonical: `include/cui/cui_iso.h` and
  `include/cui/cui_iso_opts.h` are the only copies, and the root-level
  `cui_iso.h` / `cui_iso_opts.h` are thin forwarders. A header fix is now
  made once.

The standalone repository (`thiensubs/cui-iso`) was built and tested against
a fresh clone of upstream CUI master, with the shipped patch applied, so the
published path is the tested path:

    git clone https://github.com/yappologistic/CUI.git
    cd CUI && git apply /path/to/cui-iso/patches/cui-iso-clip-api.patch
    cmake -S /path/to/cui-iso -B b -DCUI_ISO_CUI_DIR=$PWD -DCUI_ISO_SANITIZE=ON
    cmake --build b -j8
    ctest --test-dir b --output-on-failure

    100% tests passed out of 10

Without the patch, the configure step stops with the instruction to apply it,
instead of failing at link time.

The ten example PNGs render from the current tree with combined md5
`6264ecd85502a68f3a0b97adc1352a23`.

## Review pass 3: the 7 CodeRabbit findings

The automated reviewer returned 7 more findings after pass 2. All 7 were real
and all 7 are fixed. Every finding, the fix and the file that carries it are
in `extensions/cui-iso/AUDIT.md`, one row each.

What they were:

- The extension configured the CUI source tree a second time when the host app
  already had a `cui::cui` target. CMake refuses to create the same target
  twice, so the extension now reuses the target that is already there.
- The install interface hardcoded `include`, so a consumer that installed the
  headers somewhere else could not find them. It follows
  `CMAKE_INSTALL_INCLUDEDIR` now.
- The installed package accepted any CUI, and a CUI from before the clip APIs
  existed failed at link time. `find_package(cui_iso)` refuses it with a
  sentence now.
- A theme token was shaded and faded as raw bytes. A token is a sentinel, not
  a colour: its bytes are an id and its alpha is 0. Every draw path resolves
  the token before shading it.
- `cui_iso_pick` ignored the host's own clip, and used the bounds the caller
  set now rather than the bounds of the last paint. Both are fixed: the scene
  records the clip that was really in effect and the pick uses it.
- The extension's public declarations used CUI's `CUI_API`, which is
  `__declspec(dllimport)` for a consumer of a shared CUI on MSVC. The
  extension has its own `CUI_ISO_API` now.

What was done with them:

- The fixes were checked against deliberately broken code. Shading a token as
  bytes, ignoring the host's clip, picking with the pending bounds and
  declaring one function with `CUI_API` each make the matching test or the
  configure step fail, so the new assertions are not vacuous.
- The two clip APIs the extension needs are now added by the same upstream PR:
  `cui_push_clip_checked` and `cui_current_clip`, with two capability macros
  so a consumer can tell whether the CUI it found has them.
  `patches/cui-iso-clip-api.patch` carries the same change for a separate
  checkout, and applies to upstream master e2a0ad2.
- The ten example PNGs render byte for byte as before (combined md5
  `6264ecd85502a68f3a0b97adc1352a23`).

## Review pass 4: the 1 CodeRabbit finding

The reviewer returned one more finding after pass 3, on the fix commits. It was
real and it is fixed. The finding, the fix and the proof are in
`extensions/cui-iso/AUDIT.md`, one row.

- The installed package looked the CUI header up with `find_file`, which caches
  its result. A build directory that was configured against one CUI kept
  reading that CUI's header after `cui_DIR` was pointed at another one, so the
  capability check could accept a CUI without the clip APIs. The lookup clears
  its cached result now: a consumer that re-configures the same build
  directory against another CUI is checked against that CUI.

## Platforms tested

macOS, AppleClang 17. All ten tests run on the software renderer, and the
default windowed Cocoa build (library and examples) builds with zero warnings.
The OpenGL renderer path was not exercised: no test here drives a window.

**Not tested:** MSVC, MinGW, Linux, X11/SDL2/GLFW window backends, 32-bit,
`-ffast-math`. I have no machine for them, so CI would be the first to try.

## Risks and gaps, stated plainly

- **The extension needs two symbols from CUI.** `cui_push_clip_checked` and
  `cui_current_clip` are added by this PR, and `patches/cui-iso-clip-api.patch`
  carries the same change for anyone who wants it in a separate checkout. A
  CUI without those symbols cannot build the extension; the configure step
  says so in one sentence.
- **One spelling per solid, with two documented exceptions.** Every solid is
  constructed through `*_ex` and its options struct, or the `*_named` macro over
  it. The positional constructors are declared only in `cui_iso_internal.h`, for
  the library's own forwarding code and the tests that compare the two forms.
  Two positional declarations stay public because their arguments are not
  interchangeable floats: `cui_iso_text` (three floats, a string and a colour)
  and `cui_iso_ring` (count, time, bool, callback). The four hit-test predicates
  still take six to eight floats each and are recorded as the remaining
  exception in the audit.
- **Install is part of the deliverable.** `cmake --install` ships the static
  library and the public headers under `include/cui/`, and the exported target
  splits `BUILD_INTERFACE` from `INSTALL_INTERFACE` so no source path leaks.
  Verified by installing to a prefix and compiling an outside translation unit
  against that prefix alone with `-Wall -Wextra -Werror`.
- **The palette has fixed status hues.** `cui_iso_palette` derives neutrals from
  the theme, but `ok`/`warn`/`bad` are constants, on the argument that failure
  should look like failure in either theme. If a scene should take its colour
  from the theme alone, remove those three fields and read
  `CUI_COLOR_SUCCESS|WARNING|DANGER` at the call site.
- **No animation hook.** Pulses are a value the caller moves, which keeps the
  library free of time state and easy to test. If components are meant to
  animate themselves, this is the place to disagree with me.
- **Image paths are relative.** `docs/isometric-dioramas.md` refers to
  `../images/dioramas-*.png`, so its directory must sit at the same level as
  `docs/images/`.

## What I would keep from the review

The audit table caught five defects that a screenshot would not: solids drawn in
insertion order, two stack overflows, an out-of-bounds read, a palette that
vanished in light theme, and `cui_iso_pick` being unreachable after `cui_iso_end`.
Each of its rows carries the command or the line that proves it. Two rows cite
numbers that were wrong when first written; both were corrected by re-running the
command, and both corrections are in the history.

The second pass is the same kind of evidence at a larger scale. The reviewer
found 39 things; 34 were real, including a wedge that drew the faces pointing
away from the camera, picking that used a different shape than the renderer,
a clip that could take the caller's clip, and a lattice with no edges at all.
None of those show up in a screenshot of a working scene. The row for each one
in `AUDIT.md` names the code that now carries the fix.
