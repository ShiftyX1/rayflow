# =============================================================================
# RayflowDependencies.cmake
# External dependencies: raylib, EnTT, tinyxml2
# =============================================================================

include(FetchContent)

# FetchContent settings for better UX
set(FETCHCONTENT_QUIET OFF)

# -----------------------------------------------------------------------------
# Dependency versions (centralized)
# -----------------------------------------------------------------------------
set(RAYFLOW_RAYLIB_VERSION "5.5" CACHE STRING "raylib version to use")
set(RAYFLOW_ENTT_VERSION "v3.13.2" CACHE STRING "EnTT version to use")
set(RAYFLOW_TINYXML2_VERSION "10.0.0" CACHE STRING "tinyxml2 version to use")

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

# -----------------------------------------------------------------------------
# raylib
# Strategy:
#   1. Check RAYFLOW_FETCH_RAYLIB option (force FetchContent)
#   2. Try find_package(raylib)
#   3. Fallback to manual path search
#   4. Last resort: FetchContent
# -----------------------------------------------------------------------------
option(RAYFLOW_FETCH_RAYLIB "Force fetch raylib via FetchContent (skip system search)" OFF)

# Track how raylib was found for later use
set(RAYLIB_FOUND_VIA "" CACHE INTERNAL "How raylib was found")

if(NOT RAYFLOW_FETCH_RAYLIB)
    # Attempt 1: Standard find_package
    find_package(raylib ${RAYFLOW_RAYLIB_VERSION} QUIET CONFIG)
    
    if(raylib_FOUND)
        set(RAYLIB_FOUND_VIA "find_package" CACHE INTERNAL "")
        message(STATUS "Found raylib via find_package: ${raylib_VERSION}")
    else()
        # Attempt 2: Manual search for common installation paths
        message(STATUS "raylib not found via find_package, searching common paths...")
        
        if(WIN32)
            # Windows: MSYS2 UCRT64/MinGW64, vcpkg, manual install
            find_path(RAYLIB_INCLUDE_DIR 
                NAMES raylib.h 
                PATHS 
                    # MSYS2 UCRT64 (recommended)
                    C:/msys64/ucrt64/include
                    # MSYS2 MinGW64
                    C:/msys64/mingw64/include
                    # vcpkg default
                    $ENV{VCPKG_ROOT}/installed/x64-windows/include
                    # User-specified
                    $ENV{RAYLIB_PATH}/include
                    # Common manual install locations
                    C:/raylib/include
                    "C:/Program Files/raylib/include"
                NO_DEFAULT_PATH
            )
            find_library(RAYLIB_LIBRARY 
                NAMES raylib raylibdll
                PATHS 
                    C:/msys64/ucrt64/lib
                    C:/msys64/mingw64/lib
                    $ENV{VCPKG_ROOT}/installed/x64-windows/lib
                    $ENV{RAYLIB_PATH}/lib
                    C:/raylib/lib
                    "C:/Program Files/raylib/lib"
                NO_DEFAULT_PATH
            )
        elseif(APPLE)
            # macOS: Homebrew (ARM64 and x86_64), MacPorts
            find_path(RAYLIB_INCLUDE_DIR 
                NAMES raylib.h 
                PATHS 
                    # Homebrew ARM64 (Apple Silicon)
                    /opt/homebrew/opt/raylib/include
                    /opt/homebrew/include
                    # Homebrew x86_64 (Intel)
                    /usr/local/opt/raylib/include
                    /usr/local/include
                    # MacPorts
                    /opt/local/include
                NO_DEFAULT_PATH
            )
            find_library(RAYLIB_LIBRARY 
                NAMES raylib
                PATHS 
                    /opt/homebrew/opt/raylib/lib
                    /opt/homebrew/lib
                    /usr/local/opt/raylib/lib
                    /usr/local/lib
                    /opt/local/lib
                NO_DEFAULT_PATH
            )
        else()
            # Linux: system packages, manual install
            find_path(RAYLIB_INCLUDE_DIR 
                NAMES raylib.h 
                PATHS 
                    # Standard system paths
                    /usr/include
                    /usr/local/include
                    # Arch Linux / AUR
                    /usr/include/raylib
                    # Fedora COPR
                    /usr/include/raylib
                    # Manual install
                    $ENV{HOME}/.local/include
                    $ENV{RAYLIB_PATH}/include
                NO_DEFAULT_PATH
            )
            find_library(RAYLIB_LIBRARY 
                NAMES raylib
                PATHS 
                    /usr/lib
                    /usr/lib64
                    /usr/lib/x86_64-linux-gnu
                    /usr/lib/aarch64-linux-gnu
                    /usr/local/lib
                    /usr/local/lib64
                    $ENV{HOME}/.local/lib
                    $ENV{RAYLIB_PATH}/lib
                NO_DEFAULT_PATH
            )
        endif()
        
        if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
            set(RAYLIB_FOUND_VIA "manual" CACHE INTERNAL "")
            message(STATUS "Found raylib (manual search):")
            message(STATUS "  Include: ${RAYLIB_INCLUDE_DIR}")
            message(STATUS "  Library: ${RAYLIB_LIBRARY}")
            
            # Create imported target for consistent interface
            if(NOT TARGET raylib_manual)
                add_library(raylib_manual UNKNOWN IMPORTED)
                set_target_properties(raylib_manual PROPERTIES
                    IMPORTED_LOCATION "${RAYLIB_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIR}"
                )
            endif()
        endif()
    endif()
endif()

# Attempt 3: FetchContent as last resort
if(NOT raylib_FOUND AND NOT RAYLIB_FOUND_VIA STREQUAL "manual")
    if(RAYFLOW_FETCH_RAYLIB)
        message(STATUS "RAYFLOW_FETCH_RAYLIB=ON, fetching raylib via FetchContent...")
    else()
        message(STATUS "raylib not found on system, fetching via FetchContent...")
    endif()
    
    # raylib build options for static linking
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build raylib as static library" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Don't build raylib examples" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "Don't build raylib games" FORCE)
    set(CUSTOMIZE_BUILD OFF CACHE BOOL "Use default raylib build" FORCE)
    
    # Platform-specific raylib options
    if(UNIX AND NOT APPLE)
        # Linux: prefer X11 for broader compatibility
        set(USE_WAYLAND OFF CACHE BOOL "Use X11 instead of Wayland" FORCE)
    endif()
    
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG ${RAYFLOW_RAYLIB_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(raylib)
    
    set(RAYLIB_FOUND_VIA "FetchContent" CACHE INTERNAL "")
    message(STATUS "raylib ${RAYFLOW_RAYLIB_VERSION} fetched and built successfully")
endif()

# Summary
message(STATUS "=== raylib configuration ===")
message(STATUS "  Method: ${RAYLIB_FOUND_VIA}")
message(STATUS "  Version: ${RAYFLOW_RAYLIB_VERSION}")
message(STATUS "============================")

# -----------------------------------------------------------------------------
# Helper function to link raylib to a target
# Handles all three discovery methods uniformly
# -----------------------------------------------------------------------------
function(rayflow_link_raylib TARGET_NAME)
    if(RAYLIB_FOUND_VIA STREQUAL "find_package" OR RAYLIB_FOUND_VIA STREQUAL "FetchContent")
        # Modern CMake target available
        target_link_libraries(${TARGET_NAME} PUBLIC raylib)
        
        # For static linking with vcpkg on Windows, we need to link glfw3 explicitly
        if(WIN32)
            find_package(glfw3 CONFIG QUIET)
            if(glfw3_FOUND)
                target_link_libraries(${TARGET_NAME} PUBLIC glfw)
            endif()
        endif()
    elseif(RAYLIB_FOUND_VIA STREQUAL "manual")
        # Use our imported target
        target_link_libraries(${TARGET_NAME} PUBLIC raylib_manual)
    else()
        message(FATAL_ERROR "raylib not available for target ${TARGET_NAME}. "
                           "Set RAYFLOW_FETCH_RAYLIB=ON to download raylib automatically.")
    endif()
endfunction()
