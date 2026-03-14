# =============================================================================
# RayflowDependencies.cmake
# External dependencies: GLFW, glm, glad, stb, Dear ImGui, EnTT, tinyxml2, LuaJIT, sol2
# =============================================================================

include(FetchContent)
include(ExternalProject)

# FetchContent settings for better UX
set(FETCHCONTENT_QUIET OFF)

# -----------------------------------------------------------------------------
# Dependency versions (centralized)
# -----------------------------------------------------------------------------
set(RAYFLOW_GLFW_VERSION "3.4" CACHE STRING "GLFW version to use")
set(RAYFLOW_GLM_VERSION "1.0.1" CACHE STRING "glm version to use")
set(RAYFLOW_IMGUI_VERSION "v1.91.8" CACHE STRING "Dear ImGui version to use")
set(RAYFLOW_ENTT_VERSION "v3.13.2" CACHE STRING "EnTT version to use")
set(RAYFLOW_TINYXML2_VERSION "10.0.0" CACHE STRING "tinyxml2 version to use")
# Note: v3.3.1 has a bug with optional_implementation.hpp on some compilers
# Using develop branch which has the fix
set(RAYFLOW_SOL2_VERSION "develop" CACHE STRING "sol2 version to use")

# -----------------------------------------------------------------------------
# EnTT (Entity Component System)
# -----------------------------------------------------------------------------
message(STATUS "Fetching EnTT ${RAYFLOW_ENTT_VERSION}...")
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG ${RAYFLOW_ENTT_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(entt)

# -----------------------------------------------------------------------------
# TinyXML2 (XML parsing for UI)
# -----------------------------------------------------------------------------
message(STATUS "Fetching tinyxml2 ${RAYFLOW_TINYXML2_VERSION}...")
FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG ${RAYFLOW_TINYXML2_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tinyxml2)

message(STATUS "Fetching ENet...")
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG v1.3.18
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(enet)

# -----------------------------------------------------------------------------
# GLFW (Window management + input)
# -----------------------------------------------------------------------------
message(STATUS "Fetching GLFW ${RAYFLOW_GLFW_VERSION}...")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG ${RAYFLOW_GLFW_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# -----------------------------------------------------------------------------
# glm (Mathematics library, header-only)
# -----------------------------------------------------------------------------
message(STATUS "Fetching glm ${RAYFLOW_GLM_VERSION}...")
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG ${RAYFLOW_GLM_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

# -----------------------------------------------------------------------------
# glad (OpenGL loader, generated for GL 4.1 core — max on macOS)
# We embed glad sources directly; download once, commit to repo.
# -----------------------------------------------------------------------------
set(RAYFLOW_GLAD_DIR "${CMAKE_SOURCE_DIR}/engine/renderer/glad")
if(NOT EXISTS "${RAYFLOW_GLAD_DIR}/src/gl.c")
    message(FATAL_ERROR
        "glad sources not found at ${RAYFLOW_GLAD_DIR}. "
        "Run: python -m glad --api gl:core=4.1 --out-path engine/renderer/glad c")
endif()

add_library(glad SHARED "${RAYFLOW_GLAD_DIR}/src/gl.c")
target_include_directories(glad PUBLIC "${RAYFLOW_GLAD_DIR}/include")
target_compile_definitions(glad
    PUBLIC  GLAD_API_CALL_EXPORT
    PRIVATE GLAD_API_CALL_EXPORT_BUILD
)

# -----------------------------------------------------------------------------
# stb (image + truetype, header-only — implementation compiled in engine)
# -----------------------------------------------------------------------------
message(STATUS "Fetching stb...")
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
target_include_directories(stb_headers INTERFACE ${stb_SOURCE_DIR})

# -----------------------------------------------------------------------------
# Dear ImGui (debug UI)
# We build it as a static library with GLFW+OpenGL3 backends.
# -----------------------------------------------------------------------------
message(STATUS "Fetching Dear ImGui ${RAYFLOW_IMGUI_VERSION}...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG ${RAYFLOW_IMGUI_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw glad)

# -----------------------------------------------------------------------------
# LuaJIT (Lua scripting with JIT compilation)
# Strategy: Always build from source via ExternalProject for consistent cross-platform builds
# This avoids issues with mismatched system LuaJIT (e.g., MSYS2 GCC vs MSVC)
# -----------------------------------------------------------------------------
set(RAYFLOW_LUAJIT_VERSION "v2.1" CACHE STRING "LuaJIT version to use")

message(STATUS "Fetching LuaJIT ${RAYFLOW_LUAJIT_VERSION}...")

set(LUAJIT_SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/luajit-src)
set(LUAJIT_INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/luajit-install)

# Ensure install directories exist
file(MAKE_DIRECTORY ${LUAJIT_INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${LUAJIT_INSTALL_DIR}/lib)

if(WIN32)
    if(MSVC)
        # Windows MSVC: Build static library using msvcbuild.bat
        # msvcbuild.bat requires Visual Studio environment, which is already set up
        # when running cmake --build from Developer Command Prompt or via vcvarsall.bat
        set(LUAJIT_LIBRARY_FILE ${LUAJIT_INSTALL_DIR}/lib/lua51.lib)
        
        # Create a wrapper script that calls msvcbuild.bat with proper environment
        # The environment variables are inherited from the parent cmake process
        set(LUAJIT_BUILD_SCRIPT "${CMAKE_BINARY_DIR}/_deps/build_luajit.bat")
        file(WRITE ${LUAJIT_BUILD_SCRIPT} 
"@echo off
cd /d \"${LUAJIT_SOURCE_DIR}/src\"
call msvcbuild.bat static
")
        
        ExternalProject_Add(luajit_external
            GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
            GIT_TAG ${RAYFLOW_LUAJIT_VERSION}
            GIT_SHALLOW TRUE
            SOURCE_DIR ${LUAJIT_SOURCE_DIR}
            BINARY_DIR ${LUAJIT_SOURCE_DIR}/src
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ${LUAJIT_BUILD_SCRIPT}
            INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lua51.lib ${LUAJIT_INSTALL_DIR}/lib/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lua.h ${LUAJIT_INSTALL_DIR}/include/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/luajit.h ${LUAJIT_INSTALL_DIR}/include/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/luaconf.h ${LUAJIT_INSTALL_DIR}/include/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lualib.h ${LUAJIT_INSTALL_DIR}/include/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lauxlib.h ${LUAJIT_INSTALL_DIR}/include/
            BUILD_BYPRODUCTS ${LUAJIT_LIBRARY_FILE}
        )
    else()
        # Windows MinGW: Build static library using make
        set(LUAJIT_LIBRARY_FILE ${LUAJIT_INSTALL_DIR}/lib/libluajit.a)
        
        ExternalProject_Add(luajit_external
            GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
            GIT_TAG ${RAYFLOW_LUAJIT_VERSION}
            GIT_SHALLOW TRUE
            SOURCE_DIR ${LUAJIT_SOURCE_DIR}
            BINARY_DIR ${LUAJIT_SOURCE_DIR}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ${CMAKE_COMMAND} -E env "MAKE=mingw32-make" mingw32-make -C src BUILDMODE=static
            INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/libluajit.a ${LUAJIT_INSTALL_DIR}/lib/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lua.h ${LUAJIT_INSTALL_DIR}/include/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/luajit.h ${LUAJIT_INSTALL_DIR}/include/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/luaconf.h ${LUAJIT_INSTALL_DIR}/include/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lualib.h ${LUAJIT_INSTALL_DIR}/include/
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lauxlib.h ${LUAJIT_INSTALL_DIR}/include/
            BUILD_BYPRODUCTS ${LUAJIT_LIBRARY_FILE}
        )
    endif()
elseif(APPLE)
    # macOS: Build static library
    # LuaJIT requires MACOSX_DEPLOYMENT_TARGET to be set
    set(LUAJIT_LIBRARY_FILE ${LUAJIT_INSTALL_DIR}/lib/libluajit-5.1.a)
    
    # Determine architecture for cross-compilation
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(LUAJIT_TARGET_FLAGS "TARGET_FLAGS=-arch arm64")
    else()
        set(LUAJIT_TARGET_FLAGS "")
    endif()
    
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(LUAJIT_MACOSX_TARGET "11.0")
    else()
        set(LUAJIT_MACOSX_TARGET "10.15")
    endif()
    
    ExternalProject_Add(luajit_external
        GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
        GIT_TAG ${RAYFLOW_LUAJIT_VERSION}
        GIT_SHALLOW TRUE
        SOURCE_DIR ${LUAJIT_SOURCE_DIR}
        BINARY_DIR ${LUAJIT_SOURCE_DIR}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${CMAKE_COMMAND} -E env MACOSX_DEPLOYMENT_TARGET=${LUAJIT_MACOSX_TARGET}
            make -C src BUILDMODE=static ${LUAJIT_TARGET_FLAGS}
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/libluajit.a ${LUAJIT_INSTALL_DIR}/lib/libluajit-5.1.a
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lua.h ${LUAJIT_INSTALL_DIR}/include/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/luajit.h ${LUAJIT_INSTALL_DIR}/include/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/luaconf.h ${LUAJIT_INSTALL_DIR}/include/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lualib.h ${LUAJIT_INSTALL_DIR}/include/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lauxlib.h ${LUAJIT_INSTALL_DIR}/include/
        BUILD_BYPRODUCTS ${LUAJIT_LIBRARY_FILE}
    )
else()
    # Linux: Build static library
    set(LUAJIT_LIBRARY_FILE ${LUAJIT_INSTALL_DIR}/lib/libluajit-5.1.a)
    
    ExternalProject_Add(luajit_external
        GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
        GIT_TAG ${RAYFLOW_LUAJIT_VERSION}
        GIT_SHALLOW TRUE
        SOURCE_DIR ${LUAJIT_SOURCE_DIR}
        BINARY_DIR ${LUAJIT_SOURCE_DIR}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make -C src BUILDMODE=static XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/libluajit.a ${LUAJIT_INSTALL_DIR}/lib/libluajit-5.1.a
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lua.h ${LUAJIT_INSTALL_DIR}/include/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/luajit.h ${LUAJIT_INSTALL_DIR}/include/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/luaconf.h ${LUAJIT_INSTALL_DIR}/include/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lualib.h ${LUAJIT_INSTALL_DIR}/include/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_SOURCE_DIR}/src/lauxlib.h ${LUAJIT_INSTALL_DIR}/include/
        BUILD_BYPRODUCTS ${LUAJIT_LIBRARY_FILE}
    )
endif()

# Create INTERFACE library target for LuaJIT
# INTERFACE avoids Ninja issue where IMPORTED library file must exist at configure time
add_library(luajit_lib INTERFACE)
target_include_directories(luajit_lib INTERFACE ${LUAJIT_INSTALL_DIR}/include)
target_link_libraries(luajit_lib INTERFACE ${LUAJIT_LIBRARY_FILE})
add_dependencies(luajit_lib luajit_external)

# Platform-specific link dependencies
if(UNIX AND NOT APPLE)
    target_link_libraries(luajit_lib INTERFACE dl m)
endif()

# Create luajit::luajit alias for consistency
add_library(luajit::luajit ALIAS luajit_lib)

message(STATUS "=== LuaJIT configuration ===")
message(STATUS "  Method: ExternalProject (build from source)")
message(STATUS "  Version: ${RAYFLOW_LUAJIT_VERSION}")
message(STATUS "  Library: ${LUAJIT_LIBRARY_FILE}")
message(STATUS "============================")

# -----------------------------------------------------------------------------
# sol2 (Modern C++ binding for Lua/LuaJIT)
# sol2 is header-only. We disable its Lua search and provide our own LuaJIT.
# -----------------------------------------------------------------------------
message(STATUS "Fetching sol2 ${RAYFLOW_SOL2_VERSION}...")
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG ${RAYFLOW_SOL2_VERSION}
    GIT_SHALLOW TRUE
)

# Disable sol2's Lua/LuaJIT search - we provide our own
set(SOL2_BUILD_LUA FALSE CACHE BOOL "" FORCE)
set(SOL2_LUA_VERSION "" CACHE STRING "" FORCE)

FetchContent_GetProperties(sol2)
if(NOT sol2_POPULATED)
    FetchContent_Populate(sol2)
endif()

# Create a header-only interface target for sol2 (don't use sol2's CMakeLists.txt)
add_library(sol2_headers INTERFACE)
target_include_directories(sol2_headers INTERFACE ${sol2_SOURCE_DIR}/include)
target_compile_definitions(sol2_headers INTERFACE SOL_ALL_SAFETIES_ON=1)

# Create sol2::sol2 alias that includes both headers and our LuaJIT
add_library(sol2::sol2 ALIAS sol2_headers)

# -----------------------------------------------------------------------------
# Helper function to link LuaJIT and sol2 to a target
# -----------------------------------------------------------------------------
function(rayflow_link_lua TARGET_NAME)
    target_link_libraries(${TARGET_NAME} PUBLIC luajit::luajit sol2::sol2)
    # Ensure the target is built after LuaJIT (always using ExternalProject now)
    add_dependencies(${TARGET_NAME} luajit_external)
endfunction()

