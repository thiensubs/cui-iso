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

## Disclosure

    Author:     AI agent
    AI model:   Kimi K2 (kimi-k2-0905-preview) via NVIDIA NIM
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
    100% tests passed out of 9

Two further checks, both mechanical rather than visual:

- Every example compiles against the public headers alone with
  `-Wall -Wextra -Werror`; none of them includes `cui_iso_internal.h`.
- Converting the examples to the named forms changed no output: the ten
  rendered PNGs are byte-identical before and after, combined md5
  `a1e4b5f5864aa5965aacec414f06c1c0` either way.
- `cmake --install` to a prefix, then a translation unit that includes only
  `<cui/cui_iso.h>` and `<cui/cui_iso_opts.h>` compiles against that prefix
  with `-Wall -Wextra -Werror`. Before the install rules, the prefix
  contained no headers at all.

The nine tests, and the negative control each was measured against:

| Test | Asserts | Negative control |
|---|---|---|
| `cui_iso_smoke` | every primitive draws and changes pixels | - |
| `cui_iso_limits` | max tessellation, capacity, degenerate geometry | before the fix ASan reported `stack-buffer-overflow`, exit 134 |
| `cui_iso_sort` | insertion order cannot change pixels | sort disabled: 12502 bytes differ |
| `cui_iso_pick` | painted pixels and hit-tested pixels agree | before: 6466 dead of 11931 (box), 2083 of 8291 (cylinder) |
| `cui_iso_opts` | named-field forms match positional, byte for byte | - |
| `cui_iso_null` | a NULL scene draws nothing | before: ASan SEGV at `cui_iso.c:164` |
| `cui_iso_palette` | stacked faces stay distinct in light and in dark | old palette: 4 failures |
| `cui_iso_layout` | named and positional layouts agree, bit for bit | - |
| `cui_iso_named` | all fourteen solids draw from named fields through their macro | stops compiling if a struct field is renamed or missed |

Also compiled with the exact command CI uses for the single-file
distribution (`-Werror`, `tests/windows_sdk_words.h` force-included): no
errors, no warnings.

Size, against this project's own rule, on the same machine with
`-Os -DNDEBUG -Wl,-dead_strip`:

    examples/hello as is        291648
    minimal CUI app             274296
    same app using cui-iso      291488

Marginal cost 17192 bytes (16.8 KiB, about 6% of hello). The diorama app is 160
bytes smaller than hello, because dead stripping drops the CUI paths it does
not use.

Screenshots, light and dark: `docs/images/dioramas-light.png` and
`dioramas-dark.png`. Numeric check, because the author cannot view images: page
corners are 255,255,255 and 18,18,18, and 93.0% / 97.3% of sampled pixels fall on
those sides. A reviewer should look anyway.

## Platforms tested

macOS, AppleClang 17. All nine tests run on the software renderer, and the
default windowed Cocoa build (library and examples) builds with zero warnings.
The OpenGL renderer path was not exercised: no test here drives a window.

**Not tested:** MSVC, MinGW, Linux, X11/SDL2/GLFW window backends, 32-bit,
`-ffast-math`. I have no machine for them, so CI would be the first to try.

## Risks and gaps, stated plainly

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
  library free of time state and easy to test. If components are meant to animate
  themselves, this is the place to disagree with me.
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
