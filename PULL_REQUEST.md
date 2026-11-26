# Upstream history

The work in this repository was first sent to CUI upstream as pull requests.
Those pull requests are closed; the owner of CUI read the original pull request
and asked for it to be split into smaller pieces, the work was reworked to
that shape, and then the pull requests were closed. The record is kept here so
the reasoning is not lost with them.

| piece | upstream pull request | state now |
|---|---|---|
| the install fix (`CMAKE_INSTALL_INCLUDEDIR`) | yappologistic/CUI#135 | closed; the owner applied the same diff to CUI master as `d93d596` and `f3ab64d`, with our authorship |
| the clip API (`cui_push_clip_checked`, `cui_current_clip`) | yappologistic/CUI#137 | closed; the code lives in `thiensubs/CUI`, branch `feature/clip-api`, and in `patches/cui-iso-clip-api.patch` |
| the workflow action bumps (Node 20 to Node 24) | yappologistic/CUI#136 | closed; the owner applied the same diff to CUI master as `08e4ac3`, with our authorship |
| the original combined pull request | yappologistic/CUI#129 | closed as superseded; the extension moved to this repository |

## Where the code lives now

- `thiensubs/CUI`, branch `feature/clip-api`: CUI plus the two clip APIs.
  Its `master` branch tracks upstream CUI master.
- `thiensubs/cui-iso` (this repository): the extension, which finds CUI with
  `find_package(cui)`.

## What the owner's review changed

1. The install fix became its own change: one file, `CMakeLists.txt`, with
   `$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>`.
2. The clip API lost its capability macros. The owner would rather settle the
   `CUI_HAS_*` convention on its own than as a side effect of an extension, so
   the one gate is a read of the header of the CUI that was found.
3. The workflow bumps became their own change: 16 `uses:` lines, each action
   checked against its own release notes before it was named.
4. The extension moved out of the CUI tree into a repository of its own that
   finds CUI the way a consumer does: `find_package(cui)`.

The verification behind each of these is in `AUDIT.md`, one row per finding,
and the CI workflow runs the same checks on every push.
