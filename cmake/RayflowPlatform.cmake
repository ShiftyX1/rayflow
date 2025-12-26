# =============================================================================
# RayflowPlatform.cmake
# Platform-specific settings and linking
# =============================================================================

# -----------------------------------------------------------------------------
# Platform detection
# -----------------------------------------------------------------------------
if(WIN32)
    set(RAYFLOW_PLATFORM "Windows")
elseif(APPLE)
    set(RAYFLOW_PLATFORM "macOS")
elseif(UNIX)
    set(RAYFLOW_PLATFORM "Linux")
else()
    set(RAYFLOW_PLATFORM "Unknown")
endif()

message(STATUS "Target platform: ${RAYFLOW_PLATFORM}")

# -----------------------------------------------------------------------------
# Helper function to add platform-specific libraries to a target
# -----------------------------------------------------------------------------
function(rayflow_link_platform TARGET_NAME)
    if(APPLE)
        target_link_libraries(${TARGET_NAME} PRIVATE
            "-framework IOKit"
            "-framework Cocoa"
            "-framework OpenGL"
        )
    elseif(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE
            winmm
            gdi32
        )
        if(CYGWIN OR MINGW)
            target_link_libraries(${TARGET_NAME} PRIVATE
                opengl32
                user32
            )
        endif()
    elseif(UNIX)
        # Linux
        find_package(OpenGL REQUIRED)
        target_link_libraries(${TARGET_NAME} PRIVATE
            ${OPENGL_LIBRARIES}
            m
            pthread
            dl
        )
        
        # X11 dependencies (for non-Wayland)
        find_package(X11 QUIET)
        if(X11_FOUND)
            target_link_libraries(${TARGET_NAME} PRIVATE ${X11_LIBRARIES})
        endif()
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Executable properties per platform
# -----------------------------------------------------------------------------
function(rayflow_configure_executable TARGET_NAME)
    if(WIN32 AND MSVC)
        # Windows MSVC: Configure subsystem and entry point
        # For Release: GUI app (no console window) but using main() entry point
        # For Debug: Console app for easier debugging
        if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            target_link_options(${TARGET_NAME} PRIVATE
                "/SUBSYSTEM:WINDOWS"
                "/ENTRY:mainCRTStartup"
            )
        else()
            target_link_options(${TARGET_NAME} PRIVATE
                "/SUBSYSTEM:CONSOLE"
            )
        endif()
    elseif(APPLE)
        # macOS: will be configured for .app bundle in packaging
        set_target_properties(${TARGET_NAME} PROPERTIES
            MACOSX_BUNDLE FALSE  # Will be TRUE only for Release packaging
        )
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Install RPATH settings
# -----------------------------------------------------------------------------
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
elseif(UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()
