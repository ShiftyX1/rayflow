cmake_minimum_required(VERSION 3.15)
project(low_pulse C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

find_package(raylib QUIET)

if (NOT raylib_FOUND)
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
    
    if (RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
        message(STATUS "Found raylib: ${RAYLIB_LIBRARY}")
        include_directories(${RAYLIB_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "raylib not found! Please install it via: brew install raylib")
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/lib)

set(SOURCES
    src/main.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

if (raylib_FOUND)
    target_link_libraries(${PROJECT_NAME} raylib)
else()
    target_link_libraries(${PROJECT_NAME} ${RAYLIB_LIBRARY})
endif()

if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()