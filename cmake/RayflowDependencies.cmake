# =============================================================================
# RayflowDependencies.cmake
# External dependencies: raylib, EnTT, tinyxml2
# =============================================================================

include(FetchContent)

# -----------------------------------------------------------------------------
# EnTT (Entity Component System)
# -----------------------------------------------------------------------------
message(STATUS "Fetching EnTT...")
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(entt)

# -----------------------------------------------------------------------------
# TinyXML2 (XML parsing for UI)
# -----------------------------------------------------------------------------
message(STATUS "Fetching tinyxml2...")
FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG 10.0.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tinyxml2)

# -----------------------------------------------------------------------------
# raylib
# -----------------------------------------------------------------------------
option(RAYFLOW_FETCH_RAYLIB "Force fetch raylib via FetchContent" OFF)

set(RAYLIB_VERSION "5.0")

if(NOT RAYFLOW_FETCH_RAYLIB)
    # Try to find system-installed raylib
    find_package(raylib ${RAYLIB_VERSION} QUIET)
    
    if(NOT raylib_FOUND)
        # Fallback: manual search for common installation paths
        if(WIN32)
            find_path(RAYLIB_INCLUDE_DIR 
                NAMES raylib.h 
                PATHS 
                    C:/msys64/ucrt64/include
                    C:/msys64/mingw64/include
                    $ENV{RAYLIB_PATH}/include
            )
            find_library(RAYLIB_LIBRARY 
                NAMES raylib libraylib raylib_static
                PATHS 
                    C:/msys64/ucrt64/lib
                    C:/msys64/mingw64/lib
                    $ENV{RAYLIB_PATH}/lib
            )
        elseif(APPLE)
            find_path(RAYLIB_INCLUDE_DIR 
                NAMES raylib.h 
                PATHS 
                    /opt/homebrew/opt/raylib/include
                    /usr/local/opt/raylib/include
                    /opt/homebrew/include
                    /usr/local/include
            )
            find_library(RAYLIB_LIBRARY 
                NAMES raylib
                PATHS 
                    /opt/homebrew/opt/raylib/lib
                    /usr/local/opt/raylib/lib
                    /opt/homebrew/lib
                    /usr/local/lib
            )
        else()
            # Linux
            find_path(RAYLIB_INCLUDE_DIR 
                NAMES raylib.h 
                PATHS 
                    /usr/include
                    /usr/local/include
            )
            find_library(RAYLIB_LIBRARY 
                NAMES raylib
                PATHS 
                    /usr/lib
                    /usr/lib/x86_64-linux-gnu
                    /usr/local/lib
            )
        endif()
        
        if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
            message(STATUS "Found raylib (manual): ${RAYLIB_LIBRARY}")
            set(RAYLIB_FOUND_MANUAL TRUE)
        endif()
    endif()
endif()

# If not found anywhere, fetch via FetchContent
if(NOT raylib_FOUND AND NOT RAYLIB_FOUND_MANUAL)
    message(STATUS "raylib not found on system, fetching via FetchContent...")
    
    # raylib build options for static linking
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG ${RAYLIB_VERSION}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
    
    set(RAYLIB_FETCHED TRUE)
    message(STATUS "raylib ${RAYLIB_VERSION} fetched successfully")
else()
    if(raylib_FOUND)
        message(STATUS "Found raylib via find_package: ${raylib_VERSION}")
    endif()
endif()

# -----------------------------------------------------------------------------
# Helper function to link raylib to a target
# -----------------------------------------------------------------------------
function(rayflow_link_raylib TARGET_NAME)
    if(raylib_FOUND OR RAYLIB_FETCHED)
        target_link_libraries(${TARGET_NAME} PUBLIC raylib)
    elseif(RAYLIB_FOUND_MANUAL)
        target_include_directories(${TARGET_NAME} PUBLIC ${RAYLIB_INCLUDE_DIR})
        target_link_libraries(${TARGET_NAME} PUBLIC ${RAYLIB_LIBRARY})
    else()
        message(FATAL_ERROR "raylib not available for target ${TARGET_NAME}")
    endif()
endfunction()
