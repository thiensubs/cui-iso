cmake_minimum_required(VERSION 3.14)
project(cui_iso_demo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/CUI ${CMAKE_CURRENT_BINARY_DIR}/cui)
include(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/CUI/cmake/CuiApp.cmake)

# The reusable layer. Build it as a library so an app links cui_iso and gets
# the whole diorama API; nothing here depends on mcpd-ui.
add_library(cui_iso cui_iso.c)
target_include_directories(cui_iso PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cui_iso PUBLIC cui::cui)

# Headless proof: renders a scene with the software renderer and writes a PNG.
# This is what lets us diff the lib's output against the hand-rolled original.
add_executable(cui_iso_render render_main.c)
target_link_libraries(cui_iso_render PRIVATE cui_iso)

# The API-usability test: a grid of cells, each one primitive + a caption.
# Adding a case is one table row plus a few lines; if that is awkward, the
# library is too hard to use.
add_executable(cui_iso_cases cases.c)
target_link_libraries(cui_iso_cases PRIVATE cui_iso)

# Smoke test, wired into CTest: renders every primitive headlessly and asserts
# it draws without crashing and changes pixels. This is what CI gates on.
add_executable(cui_iso_test test_cui_iso.c)
target_link_libraries(cui_iso_test PRIVATE cui_iso)
enable_testing()
add_test(NAME cui_iso_smoke COMMAND cui_iso_test)

# Boundary test: pushes every bounded loop to its maximum (cylinder sides 64,
# lattice 24 nodes, more solids than fit, layouts at 0/1) and
# checks degenerate geometry. Paired with the sanitizers is the real gate.
add_executable(cui_iso_limits_test test_cui_iso_limits.c)
target_link_libraries(cui_iso_limits_test PRIVATE cui_iso)
add_test(NAME cui_iso_limits COMMAND cui_iso_limits_test)

# Sort invariant: the same solids added in opposite order must render identical
# pixels. If cui_iso_end stops sorting, this fails.
add_executable(cui_iso_sort_test test_cui_iso_sort.c)
target_link_libraries(cui_iso_sort_test PRIVATE cui_iso)
add_test(NAME cui_iso_sort COMMAND cui_iso_sort_test)

# Picking vs silhouette: every painted pixel of a lone solid must be hit by
# that solid hit-test, and no unpainted pixel may be. Catches a hit shape
# that drifts from the drawn shape.
add_executable(cui_iso_pick_test test_cui_iso_pick.c)
target_link_libraries(cui_iso_pick_test PRIVATE cui_iso)
add_test(NAME cui_iso_pick COMMAND cui_iso_pick_test)

# Sanitized variants, for the gate CI should run too.
option(CUI_ISO_SANITIZE "Build the tests with ASan and UBSan" OFF)
if(CUI_ISO_SANITIZE AND NOT MSVC)
    foreach(t cui_iso_test cui_iso_limits_test cui_iso_sort_test cui_iso_pick_test)
        target_compile_options(${t} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${t} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

# A real-world example in CUI's own example shape: a CI/CD pipeline diorama.
add_executable(pipeline examples/pipeline.c)
target_link_libraries(pipeline PRIVATE cui_iso)
cui_app_target(pipeline)

# Ten real-world cases rendered to PNGs, for the contact sheet.
add_executable(top10 examples/top10.c)
target_link_libraries(top10 PRIVATE cui_iso)

# A windowed demo: the mcpd-style hub-and-pods scene rebuilt on top of cui_iso.
add_executable(cui_iso_demo scene_demo.c)
target_link_libraries(cui_iso_demo PRIVATE cui_iso)
cui_app_target(cui_iso_demo)
