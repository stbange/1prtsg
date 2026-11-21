# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.24)
project(1prtsg VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ENABLE_COVERAGE "Enable GCC/Clang gcov coverage instrumentation" OFF)
option(ENABLE_SANITIZERS "Enable ASan and UBSan on supported compilers" OFF)
option(BUILD_TESTING "Build C unit tests and register CTest targets" ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Third-Party Dependencies (Permissive Open-Source Licenses Only)
# ---------------------------------------------------------------------------

# 1. SDL3 (zlib license) - Static build with tests/examples disabled for fast builds
if(NOT EMSCRIPTEN)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(SDL_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.30
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
else()
    # Emscripten builds can also build SDL3 statically from source
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.30
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

# 2. Lua 5.4 (MIT License)
FetchContent_Declare(
    lua54
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.8
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(lua54)
if(NOT lua54_POPULATED)
    FetchContent_Populate(lua54)
endif()

set(LUA_SRC_DIR "${lua54_SOURCE_DIR}")
if(EXISTS "${lua54_SOURCE_DIR}/src/lapi.c")
    set(LUA_SRC_DIR "${lua54_SOURCE_DIR}/src")
endif()

set(LUA_CORE_SOURCES
    "${LUA_SRC_DIR}/lapi.c"
    "${LUA_SRC_DIR}/lcode.c"
    "${LUA_SRC_DIR}/lctype.c"
    "${LUA_SRC_DIR}/ldebug.c"
    "${LUA_SRC_DIR}/ldo.c"
    "${LUA_SRC_DIR}/ldump.c"
    "${LUA_SRC_DIR}/lfunc.c"
    "${LUA_SRC_DIR}/lgc.c"
    "${LUA_SRC_DIR}/llex.c"
    "${LUA_SRC_DIR}/lmem.c"
    "${LUA_SRC_DIR}/lobject.c"
    "${LUA_SRC_DIR}/lopcodes.c"
    "${LUA_SRC_DIR}/lparser.c"
    "${LUA_SRC_DIR}/lstate.c"
    "${LUA_SRC_DIR}/lstring.c"
    "${LUA_SRC_DIR}/ltable.c"
    "${LUA_SRC_DIR}/ltm.c"
    "${LUA_SRC_DIR}/lundump.c"
    "${LUA_SRC_DIR}/lvm.c"
    "${LUA_SRC_DIR}/lzio.c"
    "${LUA_SRC_DIR}/lauxlib.c"
    "${LUA_SRC_DIR}/lbaselib.c"
    "${LUA_SRC_DIR}/lcorolib.c"
    "${LUA_SRC_DIR}/ldblib.c"
    "${LUA_SRC_DIR}/liolib.c"
    "${LUA_SRC_DIR}/lmathlib.c"
    "${LUA_SRC_DIR}/loadlib.c"
    "${LUA_SRC_DIR}/loslib.c"
    "${LUA_SRC_DIR}/lstrlib.c"
    "${LUA_SRC_DIR}/ltablib.c"
    "${LUA_SRC_DIR}/lutf8lib.c"
    "${LUA_SRC_DIR}/linit.c"
)

add_library(lua54_static STATIC ${LUA_CORE_SOURCES})
target_include_directories(lua54_static PUBLIC "${LUA_SRC_DIR}")
if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    target_compile_definitions(lua54_static PRIVATE LUA_USE_POSIX)
    target_link_libraries(lua54_static PRIVATE m)
elseif(APPLE)
    target_compile_definitions(lua54_static PRIVATE LUA_USE_MACOSX)
elseif(EMSCRIPTEN)
    target_compile_definitions(lua54_static PRIVATE LUA_USE_POSIX)
endif()

# 3. stb (MIT / Public Domain) - External PNG loading & screenshot writing
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

add_library(stb_headers INTERFACE)
target_include_directories(stb_headers INTERFACE "${stb_SOURCE_DIR}")

# 4. Unity C Unit Test Framework (MIT License)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG v2.6.1
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(unity)
    if(NOT unity_POPULATED)
        FetchContent_Populate(unity)
    endif()
    add_library(unity_test STATIC "${unity_SOURCE_DIR}/src/unity.c")
    target_include_directories(unity_test PUBLIC "${unity_SOURCE_DIR}/src")
    target_compile_definitions(unity_test PUBLIC UNITY_INCLUDE_DOUBLE)
endif()

# ---------------------------------------------------------------------------
# Compiler Flags & Instrumentation Helpers
# ---------------------------------------------------------------------------
function(rts_apply_strict_flags target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Werror -Wpedantic)
    endif()
    if(ENABLE_SANITIZERS AND NOT MSVC AND NOT EMSCRIPTEN)
        target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

function(rts_apply_coverage target_name)
    if(ENABLE_COVERAGE AND NOT MSVC AND NOT EMSCRIPTEN)
        target_compile_options(${target_name} PRIVATE --coverage -O0 -g)
        target_link_options(${target_name} PUBLIC --coverage)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Core Engine Library (Platform-Independent C11, Hardware-Injected Seams)
# ---------------------------------------------------------------------------
add_library(rts_core STATIC
    src/math3d.c
    src/planet.c
    src/vfs.c
    src/interior.c
    src/world.c
    src/voxel.c
    src/input.c
    src/audio.c
    src/economy.c
    src/controller.c
    src/boarding.c
    src/config.c
    src/save_load.c
    src/obj_loader.c
    src/image_io.c
    src/gfx_null.c
    src/retro_renderer.c
    src/tactical_sim.c
    src/fun_eval.c
    src/lua_vm.c
)
target_include_directories(rts_core PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(rts_core PUBLIC lua54_static stb_headers)
if(UNIX)
    target_link_libraries(rts_core PUBLIC m)
endif()
rts_apply_strict_flags(rts_core)
rts_apply_coverage(rts_core)

# Separate stb implementation TU so third-party header warnings don't pollute strict flags
add_library(rts_stb_impl STATIC src/stb_impl.c)
target_link_libraries(rts_stb_impl PUBLIC stb_headers)
if(UNIX)
    target_link_libraries(rts_stb_impl PRIVATE m)
endif()
target_link_libraries(rts_core PUBLIC rts_stb_impl)

# ---------------------------------------------------------------------------
# Platform & Graphics Backend Library (SDL3 + OpenGL 3.3 / WebGL 2 / WebXR)
# ---------------------------------------------------------------------------
add_library(rts_platform STATIC
    src/gl_loader.c
    src/gfx_gl.c
    src/audio_sdl3.c
    src/platform_sdl.c
    src/platform_webxr.c
)
target_include_directories(rts_platform PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(rts_platform PUBLIC rts_core SDL3::SDL3-static)

if(EMSCRIPTEN)
    target_compile_definitions(rts_platform PUBLIC RTS_PLATFORM_WEB=1)
elseif(WIN32)
    target_link_libraries(rts_platform PUBLIC opengl32)
elseif(APPLE)
    target_link_libraries(rts_platform PUBLIC "-framework OpenGL")
    target_compile_definitions(rts_platform PUBLIC GL_SILENCE_DEPRECATION)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(rts_platform PUBLIC OpenGL::GL)
endif()
rts_apply_strict_flags(rts_platform)

# ---------------------------------------------------------------------------
# Main Game Executable (Zero Baked Assets - Reads Sibling Folders)
# ---------------------------------------------------------------------------
add_executable(1prtsg src/main.c)
target_link_libraries(1prtsg PRIVATE rts_platform rts_core)
rts_apply_strict_flags(1prtsg)

if(EMSCRIPTEN)
    # Zero baked assets: never pass --preload-file or --embed-file!
    # External assets/ and scripts/ are fetched from loose HTTP sibling folders by web/index.html.
    set_target_properties(1prtsg PROPERTIES SUFFIX ".js")
    target_link_options(1prtsg PRIVATE
        "-sUSE_WEBGL2=1"
        "-sFULL_ES3=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "-sEXPORTED_FUNCTIONS=['_main','_rts_web_notify_touch_device','_rts_webxr_on_session_state','_rts_webxr_on_frame']"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']"
    )
endif()

# ---------------------------------------------------------------------------
# Unit Tests & Automated Game Tests (CTest)
# ---------------------------------------------------------------------------
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    enable_testing()

    add_executable(rts_unit_tests
        tests/test_main.c
        tests/test_math3d.c
        tests/test_planet.c
        tests/test_vfs.c
        tests/test_interior.c
        tests/test_world.c
        tests/test_voxel.c
        tests/test_input.c
        tests/test_audio.c
        tests/test_economy.c
        tests/test_controller.c
        tests/test_boarding.c
        tests/test_config_and_save.c
        tests/test_assets_and_renderer.c
        tests/test_fun_eval.c
        tests/test_lua_vm.c
    )
    target_link_libraries(rts_unit_tests PRIVATE rts_core unity_test)
    target_compile_definitions(rts_unit_tests PRIVATE RTS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}")
    rts_apply_strict_flags(rts_unit_tests)
    rts_apply_coverage(rts_unit_tests)

    add_test(NAME c_unit_tests COMMAND rts_unit_tests)
    add_test(
        NAME lua_unit_tests_and_coverage
        COMMAND 1prtsg --headless --data-dir "${CMAKE_SOURCE_DIR}" --lua-coverage --min-lua-coverage 95 --test-script tests/unit/run_all.lua
    )
    add_test(
        NAME automated_game_tests_headless
        COMMAND 1prtsg --headless --data-dir "${CMAKE_SOURCE_DIR}" --test-script tests/game/run_all.lua
    )
endif()
