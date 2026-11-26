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

# Where CUI comes from, in this order:
#   - a CUI the host app already has (the alias CUI makes in its own tree, or
#     an imported package from find_package(cui));
#   - the CUI source tree CUI_ISO_CUI_DIR points at;
#   - an installed CUI, found with find_package(cui);
#   - a vendored CUI checkout beside the extension (../vendor/CUI), or the
#     tree two directories up (the extension inside a CUI checkout).
# A consumer that wants a specific CUI points CUI_ISO_CUI_DIR at it; a
# consumer that has CUI installed gets it through find_package(cui), the way
# the CUI maintainer asked for.
if(DEFINED CACHE{CUI_ISO_CUI_DIR})
    set(_cui_iso_cui_dir_given TRUE)
else()
    set(_cui_iso_cui_dir_given FALSE)
endif()
set(CUI_ISO_CUI_DIR "" CACHE PATH
    "Path to the CUI source tree whose CMake target this library links")
option(CUI_ISO_FIND_INSTALLED_CUI
    "Look for an installed CUI with find_package(cui) when no CUI source tree is given" ON)

# The CUI this extension links against. If there is a cui::cui target already
# -- the alias CUI makes in its own tree, or an imported package from
# find_package(cui) -- that one is used and no CUI is configured a second
# time: a second add_subdirectory would run add_library(cui ...) twice, and
# CMake refuses to create the same target twice. So the source tree is only
# added when there is no target to reuse.
if(TARGET cui::cui)
    set(CUI_ISO_USE_HOST_CUI TRUE)
elseif(_cui_iso_cui_dir_given)
    set(CUI_ISO_USE_HOST_CUI FALSE)
elseif(CUI_ISO_FIND_INSTALLED_CUI)
    find_package(cui CONFIG QUIET)
    if(TARGET cui::cui)
        set(CUI_ISO_USE_HOST_CUI TRUE)
    else()
        set(CUI_ISO_USE_HOST_CUI FALSE)
    endif()
else()
    set(CUI_ISO_USE_HOST_CUI FALSE)
endif()

if(NOT CUI_ISO_USE_HOST_CUI)
    if(NOT CUI_ISO_CUI_DIR)
        # Two layouts are found without any -D: a vendored CUI checkout beside
        # the extension, and this extension inside a CUI checkout, where the CUI
        # source tree is two directories up. The vendored path is tried first:
        # two directories up can be any workspace, and a workspace that happens
        # to hold a CMakeLists.txt is not a CUI checkout.
        foreach(_cui_iso_cui_candidate
                "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/CUI"
                "${CMAKE_CURRENT_SOURCE_DIR}/../..")
            if(EXISTS "${_cui_iso_cui_candidate}/CMakeLists.txt")
                set(CUI_ISO_CUI_DIR "${_cui_iso_cui_candidate}" CACHE PATH
                    "Path to the CUI source tree whose CMake target this library links"
                    FORCE)
                break()
            endif()
        endforeach()
    endif()
    if(NOT EXISTS "${CUI_ISO_CUI_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "CUI not found. Install CUI so that find_package(cui) finds it, or set -DCUI_ISO_CUI_DIR=<path to the CUI source tree>.")
    endif()
    get_filename_component(CUI_ISO_CUI_DIR "${CUI_ISO_CUI_DIR}" ABSOLUTE)
endif()

# The library needs two clip APIs from CUI: cui_push_clip_checked, the clip
# push that reports whether the clip stack had room, and cui_current_clip,
# which reports the clip that is really in effect. Stock CUI has only
# cui_push_clip, which refuses silently at the 64-deep limit: the scene would
# draw outside its bounds, and the matching pop would take the caller's own
# clip. Check for the symbols here, so a CUI without them fails with a
# sentence instead of a link error.
#
# The check reads the header of the CUI that was found: the header of the
# source tree, or, for an installed CUI, the header in the include directory
# the package exports. CUI has no capability macros, so this header scan is
# the one gate.
set(_cui_iso_cui_header "")
if(NOT CUI_ISO_USE_HOST_CUI AND EXISTS "${CUI_ISO_CUI_DIR}/include/cui/cui.h")
    set(_cui_iso_cui_header "${CUI_ISO_CUI_DIR}/include/cui/cui.h")
elseif(TARGET cui::cui)
    get_target_property(_cui_iso_cui_includes cui::cui INTERFACE_INCLUDE_DIRECTORIES)
    set(_cui_iso_cui_include_dirs "")
    if(_cui_iso_cui_includes)
        foreach(_cui_iso_cui_include IN LISTS _cui_iso_cui_includes)
            # an imported package exports plain paths; a CUI from another source
            # tree exports $<BUILD_INTERFACE:...>, so the path is taken out of it
            string(REGEX REPLACE "^\\$<BUILD_INTERFACE:(.*)>$" "\\1"
                   _cui_iso_cui_include "${_cui_iso_cui_include}")
            list(APPEND _cui_iso_cui_include_dirs "${_cui_iso_cui_include}")
        endforeach()
        # find_file caches its result, so a build directory configured against
        # one CUI would keep reading that older header. Drop the cached value
        # first, so the check always reads the CUI that was found now.
        unset(_cui_iso_cui_header)
        unset(_cui_iso_cui_header CACHE)
        find_file(_cui_iso_cui_header cui/cui.h
                  PATHS ${_cui_iso_cui_include_dirs} NO_DEFAULT_PATH)
    endif()
endif()
if(NOT _cui_iso_cui_header OR NOT EXISTS "${_cui_iso_cui_header}")
    # The extension inside a CUI checkout: that CUI's tree is two directories up.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../include/cui/cui.h")
        set(_cui_iso_cui_header "${CMAKE_CURRENT_SOURCE_DIR}/../../include/cui/cui.h")
    else()
        message(FATAL_ERROR
            "The header of the CUI that was found could not be read, so the clip "
            "APIs cui-iso needs cannot be checked.")
    endif()
endif()
file(READ "${_cui_iso_cui_header}" _cui_iso_cui_header_text)
string(FIND "${_cui_iso_cui_header_text}" "cui_push_clip_checked" _cui_iso_has_checked_clip)
string(FIND "${_cui_iso_cui_header_text}" "cui_current_clip" _cui_iso_has_current_clip)
if(_cui_iso_has_checked_clip EQUAL -1 OR _cui_iso_has_current_clip EQUAL -1)
    message(FATAL_ERROR
        "The CUI that was found is missing a clip API cui-iso needs: "
        "cui_push_clip_checked() and cui_current_clip(). Use a CUI that has "
        "them: the feature/clip-api branch of thiensubs/CUI adds both to "
        "CUI, or apply "
        "'git apply patches/cui-iso-clip-api.patch' inside that CUI source "
        "tree. A CUI source tree can be chosen with -DCUI_ISO_CUI_DIR=<path>.")
endif()

if(NOT CUI_ISO_USE_HOST_CUI)
    add_subdirectory(${CUI_ISO_CUI_DIR} ${CMAKE_CURRENT_BINARY_DIR}/cui)
    include(${CUI_ISO_CUI_DIR}/cmake/CuiApp.cmake)
endif()

# The reusable layer. Build it as a library so an app links cui_iso and gets
# the whole diorama API; nothing here depends on mcpd-ui.
#
# The install layout is decided here and not at the install section: the
# export records the include directory, and a hardcoded "include" points at
# the wrong place when the consumer installs the headers somewhere else
# (-DCMAKE_INSTALL_INCLUDEDIR=headers).
include(GNUInstallDirs)

# The extension's public headers must not use CUI's import/export macro. A
# consumer of a shared CUI sees every CUI_API declaration as an import, which
# is right for CUI's functions and wrong for the extension's own: MSVC then
# refuses their definition (C2491) and the consumer looks for them in CUI's
# DLL instead of in the library it linked. The API test below proves it on
# Windows; this check catches the same mistake on every platform, by reading
# the headers. Only a declaration at the start of a line counts, so the
# comment that explains the rule does not trip it.
foreach(_cui_iso_public_header
        include/cui/cui_iso.h
        include/cui/cui_iso_opts.h
        cui_iso_internal.h)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${_cui_iso_public_header}" _cui_iso_header_text)
    string(REGEX MATCHALL "\n[ \t]*CUI_API[ \t]" _cui_iso_bare_api "${_cui_iso_header_text}")
    if(_cui_iso_bare_api)
        message(FATAL_ERROR
            "${_cui_iso_public_header} declares a function with CUI_API. The extension has its own macro, CUI_ISO_API: a consumer of a shared CUI would read this as an import and fail to link.")
    endif()
endforeach()

add_library(cui_iso STATIC cui_iso.c cui_iso_opts.c)
target_include_directories(cui_iso PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
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

# The extension's own declarations must not inherit CUI's import/export macro:
# a consumer of a shared CUI must not read them as imports. The test is
# compiled with CUI_SHARED defined, exactly as such a consumer would be, and
# defines one of the extension's functions -- MSVC rejects the definition of
# an imported symbol, so the mistake stops the build. The extension itself is
# not linked in: this file is the definition of that symbol.
add_executable(cui_iso_api_test test_cui_iso_api.c)
target_include_directories(cui_iso_api_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(cui_iso_api_test PRIVATE CUI_SHARED)
target_link_libraries(cui_iso_api_test PRIVATE cui::cui)
add_test(NAME cui_iso_api COMMAND cui_iso_api_test)


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
install(TARGETS cui_iso EXPORT cui_isoTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/cui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cui_isoTargets NAMESPACE cui_iso:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cui_iso)

# find_package(cui_iso CONFIG REQUIRED) finds that export through a config file
# named after the package, in a directory named after it too, so CMake's search
# finds lib/cmake/cui_iso/cui_isoConfig.cmake.
# the same shape CUI itself installs for find_package(cui CONFIG REQUIRED).
include(CMakePackageConfigHelpers)
# find_dependency(cui) takes any installed CUI, including one from before the
# clip APIs were added, and the failure would then be a link error at build
# time. The generated config checks the CUI it found, so the consumer gets a
# sentence instead.
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cui_isoConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cui_isoConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cui_iso)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/cui_isoConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cui_isoConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cui_isoConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cui_iso)
