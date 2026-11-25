# Changelog

## Unreleased

### Added
- Isometric diorama layer for CUI: solids, slots, picking, a theme-aware palette.
  Docs: `docs/isometric-dioramas.md`. Images in light and dark: `docs/images/`.
- `cui_iso_palette_default(ui)`: neutral surfaces derived from the theme's own
  background and foreground, so a diorama reads in light and in dark.
- `cui_iso_set_bounds(scene, rect)`: clip a hosted diorama to its canvas.
- `cui_iso_set_light(scene, light)`, `cui_iso_depth_bias_last(scene, bias)`.
- `cui_iso_scene_count(scene)`; `CUI_ISO_MAX_SOLIDS` is now in the header.
- Named-field forms beside every constructor, `cui_iso_box_ex` and friends,
  filling a `*_opts` struct with fields as named arguments.
- Slots: `cui_iso_layout_row`, `_ring`, `_grid`, `cui_iso_on`, `cui_iso_link_between`.

### Fixed
- Solids were drawn in insertion order despite the header and README saying
  they were sorted back to front. `cui_iso_end` sorts now.
- A stack buffer overflow in the cylinder at `sides = 64`, and an out-of-bounds
  read in the lattice at its node limit: both caught under ASan/UBSan.
- A label could be hung on an unrelated solid when the scene was full.
- Captions drew inside the sorted pass and could be painted over. They now draw
  in their own overlay pass.
- `cui_iso_pick` was unusable after `cui_iso_end` and chose the newest solid
  rather than the nearest; both fixed.
- `cui_iso_hit_box` tested only the lid, so 54% of a box's painted area answered
  no click; the hit shape is the drawn volume now.
- `CMakeLists.txt` built only inside one workspace; CUI is found through
  `CUI_ISO_CUI_DIR`.
- The default label colour was raw white, invisible on a light page.

### Fixed (review pass 2, 39 CodeRabbit findings)
- A wedge drew the faces that point away from the camera, so the far gable
  painted over the near roof after a half turn. Only camera-facing faces are
  drawn now.
- A cylinder's top cap was a fan from the centre, and CUI feathered the radial
  seams across it. The cap is the rim polygon now.
- Picking tested an un-chamfered box and sampled fewer rim points than the
  renderer used. The hit shape is the drawn shape now: the chamfered
  footprint, and the cylinder's real side count.
- A lattice left its first node isolated and gave a two-node lattice no edge.
  Neighbours are chosen by distance and each pair is linked once.
- A refused insertion (the scene is full) changed nothing but could still write
  a label onto the previous solid. A refused insertion is inert now.
- `cui_iso_end` popped a clip it had never pushed when the clip stack was
  full, which removed the caller's own clip. It draws nothing and pops
  nothing when the clip cannot be applied.
- Reduced motion rewrote a disabled pulse (value 2) to 0.5, switching the
  pulse on. Only a pulse already inside `[0,1]` is frozen now.
- `cui_iso_depth_bias_last` changed what `cui_iso_pick` reported after the
  frame was painted. The write is refused outside a batch now.
- A grid with a negative or oversized cell count was accepted and cast back to
  an int at draw time. The count is checked and capped at submission.
- `cui_iso_begin` with a NULL camera left the previous batch alive, so the
  next `cui_iso_end` drew the old scene. It clears the batch now.
- `cui_iso_set_reduced_motion(NULL, true)` dereferenced NULL.
- The side count and the cell count rode in a float and were cast back to an
  int at draw time, which is undefined past the int range.
- The examples ran a shell command to make their output directory, so a path
  with a space or a metacharacter was run by the shell. The directories are
  made with `mkdir(2)` now.
- The public headers were stored twice, once in `include/cui/` and once at the
  root of the sources. `include/cui/` is canonical now and the root copies are
  thin forwarders.

### Requirements
- The library needs `cui_push_clip_checked` from CUI. Upstream PR #129 adds
  it to CUI; `patches/cui-push-clip-checked.patch` adds it to any CUI
  checkout. The configure step stops with that instruction when the symbol is
  missing, instead of failing at link time.

### Tests
- `ctest` covers smoke, limits, sort order, picking against the silhouette, named-field options
  equivalence, and palette contrast in both themes.
- The sort test also asserts which of two overlapping boxes is painted on top,
  so an inverted comparator fails instead of passing.
- The null test compares two frames that differ by one refused call, so a
  label written onto another solid fails.
- The pick test asserts that the corner the chamfer cut away is not pickable.
- The limits test asserts the cylinder clamp, the lattice solid counts, the
  grid cap and the disk-stack cap.
