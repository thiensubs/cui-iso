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

### Tests
- `ctest` covers smoke, limits, sort order, picking against the silhouette, named-field options
  equivalence, and palette contrast in both themes.
