cmake_minimum_required(VERSION 3.14)
project(cui_iso_demo VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Sanitized builds, for the gate CI runs. Directory-wide, not per target: a
# sanitized library static-linked into an unsanitized executable fails to link
# (undefined __asan_init), so every target in here is built the same way.
option(CUI_ISO_SANITIZE "Build every target with ASan and UBSan" OFF)
if(CUI_ISO_SANITIZE AND NOT MSVC)
    # no recovery: a recoverable UBSan report otherwise only prints and the
    # test still passes, so the gate would not catch anything.
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()

# Where CUI lives. The default is a vendored checkout beside this directory,
# which is how it is arranged here; a standalone consumer points it at its own
# CUI checkout or install instead of copying that layout.
set(CUI_ISO_CUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/CUI" CACHE PATH
    "Path to the CUI source tree whose CMake target this library links")
if(NOT EXISTS "${CUI_ISO_CUI_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "CUI not found at ${CUI_ISO_CUI_DIR}. Set -DCUI_ISO_CUI_DIR=<path to CUI>.")
endif()

add_subdirectory(${CUI_ISO_CUI_DIR} ${CMAKE_CURRENT_BINARY_DIR}/cui)
include(${CUI_ISO_CUI_DIR}/cmake/CuiApp.cmake)

# The reusable layer. Build it as a library so an app links cui_iso and gets
# the whole diorama API; nothing here depends on mcpd-ui.
add_library(cui_iso STATIC cui_iso.c cui_iso_opts.c)
target_include_directories(cui_iso PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
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

# Named-field forms must draw exactly what the positional forms draw.
add_executable(cui_iso_opts_test test_cui_iso_opts.c)
target_link_libraries(cui_iso_opts_test PRIVATE cui_iso)
add_test(NAME cui_iso_opts COMMAND cui_iso_opts_test)

# A mis-wired client must draw nothing, not crash: every entry point is
# called with a NULL scene, and a full scene must not relabel an old solid.
add_executable(cui_iso_null_test test_cui_iso_null.c)
target_link_libraries(cui_iso_null_test PRIVATE cui_iso)
add_test(NAME cui_iso_null COMMAND cui_iso_null_test)

# The palette must keep its tonal separation in light and in dark. A theme
# that collapses two stacked surfaces into one colour passes a screenshot
# review and fails here.
add_executable(cui_iso_palette_test test_cui_iso_palette.c)
target_link_libraries(cui_iso_palette_test PRIVATE cui_iso)
add_test(NAME cui_iso_palette COMMAND cui_iso_palette_test)

# The named-argument layouts must place slots where the positional ones do.
add_executable(cui_iso_layout_test test_cui_iso_layout.c)
target_link_libraries(cui_iso_layout_test PRIVATE cui_iso)
add_test(NAME cui_iso_layout COMMAND cui_iso_layout_test)

# Every solid must be callable with named fields, the way CUI spells its own
# components. This file stops compiling if a field is renamed or missed.
add_executable(cui_iso_named_test test_cui_iso_named.c)
target_link_libraries(cui_iso_named_test PRIVATE cui_iso)
add_test(NAME cui_iso_named COMMAND cui_iso_named_test)


# A real-world example in CUI's own example shape: a CI/CD pipeline diorama.
# Both windowed examples need the CUI app layer; a CUI built with CUI_WITH_APP=OFF
# does not provide it, so they are not configured in that case.
if(CUI_WITH_APP)
add_executable(pipeline examples/pipeline.c)
target_link_libraries(pipeline PRIVATE cui_iso)
cui_app_target(pipeline)
endif()

# Ten real-world cases rendered to PNGs, for the contact sheet.
add_executable(top10 examples/top10.c)
target_link_libraries(top10 PRIVATE cui_iso)

# A windowed demo: the mcpd-style hub-and-pods scene rebuilt on top of cui_iso.
if(CUI_WITH_APP)
add_executable(cui_iso_demo scene_demo.c)
target_link_libraries(cui_iso_demo PRIVATE cui_iso)
cui_app_target(cui_iso_demo)
endif()

# Install the public headers and the static library so a consumer can
# use find_package(cui_iso) instead of add_subdirectory.
include(GNUInstallDirs)
install(TARGETS cui_iso EXPORT cui_isoTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/cui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cui_isoTargets NAMESPACE cui_iso:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cui_iso)

# find_package(cui_iso CONFIG REQUIRED) finds that export through a config file
# named after the package, in a directory named after it too, so CMake's search
# finds lib/cmake/cui_iso/cui_isoConfig.cmake.
# the same shape CUI itself installs for find_package(cui CONFIG REQUIRED).
include(CMakePackageConfigHelpers)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cui_isoConfig.cmake
    "include(CMakeFindDependencyMacro)\nfind_dependency(cui)\ninclude(\"\${CMAKE_CURRENT_LIST_DIR}/cui_isoTargets.cmake\")\n")
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/cui_isoConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cui_isoConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cui_isoConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cui_iso)
