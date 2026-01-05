# =============================================================================
# Rayflow Games
# =============================================================================
# This file automatically discovers and includes all game projects in the
# games/ directory. Each subdirectory with a CMakeLists.txt is included.
# =============================================================================

# Get list of subdirectories
file(GLOB GAME_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

foreach(GAME_DIR ${GAME_DIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${GAME_DIR}")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${GAME_DIR}/CMakeLists.txt")
            message(STATUS "Found game: ${GAME_DIR}")
            add_subdirectory(${GAME_DIR})
        endif()
    endif()
endforeach()
