cmake_minimum_required(VERSION 3.20)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" FZERO_VERSION LIMIT_COUNT 1)
project(FZeroSNESRecomp VERSION ${FZERO_VERSION} LANGUAGES C)
set(CMAKE_C_STANDARD 11)

include(CTest)
option(FZERO_BUILD_GAME "Build game hosts (requires privately generated ROM sources)" ON)
add_library(fzero_video STATIC src/fzero_video.c src/fzero_mode7.c src/fzero_replay.c src/fzero_hdma.c)
target_include_directories(fzero_video PUBLIC "${CMAKE_SOURCE_DIR}/src")
if(NOT MSVC)
    target_compile_options(fzero_video PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(fzero_video PUBLIC m)
endif()
if(BUILD_TESTING)
    add_executable(fzero_hdma_tests tests/test_hdma.c)
    target_link_libraries(fzero_hdma_tests PRIVATE fzero_video)
    add_test(NAME fzero_hdma COMMAND fzero_hdma_tests)
    add_executable(fzero_video_tests tests/test_video.c)
    target_link_libraries(fzero_video_tests PRIVATE fzero_video)
    add_test(NAME fzero_video COMMAND fzero_video_tests)
    add_executable(fzero_mode7_tests tests/test_mode7.c)
    target_link_libraries(fzero_mode7_tests PRIVATE fzero_video)
    add_test(NAME fzero_mode7 COMMAND fzero_mode7_tests)
endif()
if(NOT FZERO_BUILD_GAME)
    return()
endif()

# Both dependencies are pinned submodules of this repository (see
# .gitmodules); `bash tools/bootstrap.sh` initializes them. Point
# SNESRECOMP_ROOT at a worktree instead when developing framework changes
# alongside the game.
set(SNESRECOMP_ROOT "${CMAKE_SOURCE_DIR}/snesrecomp" CACHE PATH
    "Path to the snesrecomp checkout used by this game")
set(RECOMP_UI_ROOT "${CMAKE_SOURCE_DIR}/recomp-ui" CACHE PATH
    "Path to the shared recomp-ui launcher checkout")
set(SNESRECOMP_SDL_BACKEND "SDL3" CACHE STRING
    "Desktop SDL backend (SDL3 or SDL2)")
set(RECOMP_UI_ENABLE_MODS ON CACHE BOOL "F-Zero built-in native presentation mod" FORCE)
include(${SNESRECOMP_ROOT}/runner/runner.cmake)
include(${RECOMP_UI_ROOT}/recomp_ui.cmake)
find_package(OpenGL REQUIRED)

add_executable(FZeroRenderCapture tools/render_capture.c src/fzero_renderer.c)
target_include_directories(FZeroRenderCapture PRIVATE ${SNESRECOMP_RUNNER_INCLUDE_DIRS})
target_link_libraries(FZeroRenderCapture PRIVATE fzero_video)
if(BUILD_TESTING)
    add_executable(fzero_renderer_tests tests/test_renderer.c src/fzero_renderer.c)
    add_executable(fzero_mods_tests tests/test_mods.c src/fzero_mods.c)
    target_include_directories(fzero_mods_tests PRIVATE "${RECOMP_UI_ROOT}/src")
    target_link_libraries(fzero_mods_tests PRIVATE fzero_video)
    add_test(NAME fzero_mods COMMAND fzero_mods_tests)
    target_include_directories(fzero_renderer_tests PRIVATE ${SNESRECOMP_RUNNER_INCLUDE_DIRS})
    target_link_libraries(fzero_renderer_tests PRIVATE fzero_video)
    if(NOT MSVC)
        target_compile_options(fzero_renderer_tests PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME fzero_renderer COMMAND fzero_renderer_tests)
endif()

# Release version stamp defaults to VERSION; override explicitly for local builds.
# NOTE: quote it when configuring - PowerShell rewrites an unquoted
# -DSNESRECOMP_BUILD_VERSION=0.1.0 into "0".
set(SNESRECOMP_BUILD_VERSION "${PROJECT_VERSION}" CACHE STRING
    "Version stamp baked into crash reports")

set(FZERO_GEN_DIR "${CMAKE_SOURCE_DIR}/src/gen" CACHE PATH
    "Directory containing the private generated F-Zero C sources")
file(GLOB FZERO_GEN_SOURCES CONFIGURE_DEPENDS "${FZERO_GEN_DIR}/*.c")
if(NOT FZERO_GEN_SOURCES)
    message(FATAL_ERROR
        "${FZERO_GEN_DIR} is empty - stage a verified fzero.sfc and run "
        "`bash tools/regen.sh` before building.")
endif()

list(REMOVE_ITEM SNESRECOMP_RUNNER_SOURCES
    "${SNESRECOMP_ROOT}/runner/src/keybinds.c"
    "${SNESRECOMP_ROOT}/runner/src/host_report.c")

set(FZERO_DELUXE_GEN_DIR "${CMAKE_SOURCE_DIR}/captures/bs-deluxe/gen" CACHE PATH
    "Required private generated BS Deluxe native sources")
set(FZERO_DELUXE_MODS_DIR "${CMAKE_SOURCE_DIR}/build-deluxe/mods" CACHE PATH
    "Required imported BS Deluxe payload and credits")
if(NOT FZERO_DELUXE_GEN_DIR)
    message(FATAL_ERROR "Every F-Zero game build must include BS Deluxe. Set FZERO_DELUXE_GEN_DIR to its generated sources.")
endif()
foreach(deluxe_file bs-deluxe.dat bs-deluxe-import.json BS-Deluxe-credits.txt)
    if(NOT EXISTS "${FZERO_DELUXE_MODS_DIR}/${deluxe_file}")
        message(FATAL_ERROR "BS Deluxe is required: missing ${FZERO_DELUXE_MODS_DIR}/${deluxe_file}. Import BS Deluxe or set FZERO_DELUXE_MODS_DIR.")
    endif()
endforeach()
if(FZERO_DELUXE_GEN_DIR)
    file(GLOB DELUXE_SOURCES CONFIGURE_DEPENDS "${FZERO_DELUXE_GEN_DIR}/*.c")
    if(NOT DELUXE_SOURCES OR NOT EXISTS "${FZERO_DELUXE_GEN_DIR}/deluxe_namespace.h")
        message(FATAL_ERROR "Generate and import BS Deluxe before enabling its native module")
    endif()
    add_library(fzero_deluxe_native OBJECT ${DELUXE_SOURCES})
    target_include_directories(fzero_deluxe_native PRIVATE
        "${CMAKE_SOURCE_DIR}/recomp" ${SNESRECOMP_RUNNER_INCLUDE_DIRS})
    if(MSVC)
        target_compile_options(fzero_deluxe_native PRIVATE "/FI${FZERO_DELUXE_GEN_DIR}/deluxe_namespace.h")
    else()
        target_compile_options(fzero_deluxe_native PRIVATE -w -include "${FZERO_DELUXE_GEN_DIR}/deluxe_namespace.h")
    endif()
endif()

# The headless host keeps its own host_report stubs: it links no SDL, and
# host_report.c includes desktop/sdl_compat.h.
add_executable(FZeroSNESRecompHeadless
    ${SNESRECOMP_RUNNER_SOURCES}
    ${FZERO_GEN_SOURCES}
    src/headless_host.c
    src/headless_main.c
    src/fzero_runtime.c)

# The desktop host takes the REAL host_report.c (always-on breadcrumb ring,
# crash minidump writer, build-version stamp) and the REAL keybinds.c, so the
# launcher's Controls page and the game read the same keybinds.ini.
add_executable(FZeroSNESRecomp
    ${SNESRECOMP_RUNNER_SOURCES}
    "${SNESRECOMP_ROOT}/runner/src/host_report.c"
    "${SNESRECOMP_ROOT}/runner/src/keybinds.c"
    "${RECOMP_UI_ROOT}/src/third_party/gl_core_3_1.c"
    ${FZERO_GEN_SOURCES}
    src/sdl_main.c
    src/fzero_dlss.c
    src/fzero_mods.c
    src/fzero_runtime.c)

function(configure_fzero_target target)
    target_sources(${target} PRIVATE src/fzero_deluxe.c)
    if(FZERO_DELUXE_GEN_DIR)
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:fzero_deluxe_native>)
        target_compile_definitions(${target} PRIVATE FZERO_HAS_DELUXE=1)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/mods"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FZERO_DELUXE_MODS_DIR}/bs-deluxe.dat"
                "${FZERO_DELUXE_MODS_DIR}/bs-deluxe-import.json"
                "${FZERO_DELUXE_MODS_DIR}/BS-Deluxe-credits.txt"
                "$<TARGET_FILE_DIR:${target}>/mods"
            VERBATIM)
    endif()
    target_sources(${target} PRIVATE src/fzero_renderer.c)
    target_link_libraries(${target} PRIVATE fzero_video)
    target_include_directories(${target} PRIVATE
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/recomp"
        ${SNESRECOMP_RUNNER_INCLUDE_DIRS})
    target_compile_definitions(${target} PRIVATE
        SYSTEM_VOLUME_MIXER_AVAILABLE=0
        SNESRECOMP_TRACE=$<BOOL:${SNESRECOMP_ENABLE_TRACE}>
        SNESRECOMP_REVERSE_DEBUG=0)
    if(NOT MSVC)
        # The recompiled C is machine-generated; quiet its warnings and
        # tolerate the K&R-style implicit declarations clang rejects.
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-w;-Wno-implicit-function-declaration>)
        target_link_libraries(${target} PRIVATE m)
    endif()
    target_link_libraries(${target} PRIVATE ${SNESRECOMP_RUNNER_LIBRARIES})
endfunction()

configure_fzero_target(FZeroSNESRecompHeadless)
configure_fzero_target(FZeroSNESRecomp)
# Hosts include "desktop/sdl_compat.h" rather than <SDL.h> so both majors build.
snesrecomp_target_glsl_shader(FZeroSNESRecomp)
snesrecomp_target_sdl(FZeroSNESRecomp)
target_link_libraries(FZeroSNESRecomp PRIVATE OpenGL::GL)
recomp_target_launcher_ui(FZeroSNESRecomp CONSOLE snes)
configure_file(tools/dlss_worker.py dlss_worker.py COPYONLY)
add_custom_command(TARGET FZeroSNESRecomp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/img/boxart.tga"
        "$<TARGET_FILE_DIR:FZeroSNESRecomp>/assets/img/boxart.tga"
    VERBATIM)
add_custom_command(TARGET FZeroSNESRecomp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "$<TARGET_FILE_DIR:FZeroSNESRecomp>/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
        "$<TARGET_FILE_DIR:FZeroSNESRecomp>/assets/shaders"
    COMMENT "Staging F-Zero shader presets"
    VERBATIM)

if(SNESRECOMP_BUILD_VERSION)
    target_compile_definitions(FZeroSNESRecomp PRIVATE
        SNESRECOMP_BUILD_VERSION="${SNESRECOMP_BUILD_VERSION}")
endif()
