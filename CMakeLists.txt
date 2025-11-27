cmake_minimum_required(VERSION 3.15)

if(WIN32 AND NOT DEFINED CMAKE_C_COMPILER)
    if(EXISTS "C:/msys64/mingw64/bin/gcc.exe")
        set(CMAKE_C_COMPILER "C:/msys64/mingw64/bin/gcc.exe")
        set(CMAKE_MAKE_PROGRAM "C:/msys64/mingw64/bin/mingw32-make.exe")
    endif()
endif()

project(low_pulse C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

find_package(raylib QUIET)

if (NOT raylib_FOUND)
    if (WIN32)
        find_path(RAYLIB_INCLUDE_DIR 
            NAMES raylib.h 
            PATHS 
                C:/msys64/ucrt64/include
        )
        
        find_library(RAYLIB_LIBRARY 
            NAMES raylib libraylib
            PATHS 
                C:/msys64/ucrt64/lib
        )
    else()
        find_path(RAYLIB_INCLUDE_DIR 
            NAMES raylib.h 
            PATHS 
                /opt/homebrew/opt/raylib/include
                /usr/local/include
                /usr/include
        )
        
        find_library(RAYLIB_LIBRARY 
            NAMES raylib
            PATHS 
                /opt/homebrew/opt/raylib/lib
                /usr/local/lib
                /usr/lib
        )
    endif()
    
    if (RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
        message(STATUS "Found raylib: ${RAYLIB_LIBRARY}")
        include_directories(${RAYLIB_INCLUDE_DIR})
    else()
        if (WIN32)
            message(FATAL_ERROR "raylib not found! Please install it via: pacman -S mingw-w64-x86_64-raylib")
        else()
            message(FATAL_ERROR "raylib not found! Please install it via: brew install raylib")
        endif()
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/lib)

set(SOURCES
    src/main.c
    src/scene/scene.c
    src/scene/entity.c
    src/renderer/mesh.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

if (raylib_FOUND)
    target_link_libraries(${PROJECT_NAME} raylib)
else()
    target_link_libraries(${PROJECT_NAME} ${RAYLIB_LIBRARY})
endif()

if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif (WIN32)
    if (CYGWIN)
        target_link_libraries(${PROJECT_NAME} winmm gdi32 opengl32 user32)
    else()
        target_link_libraries(${PROJECT_NAME} winmm gdi32)
    endif()
endif()