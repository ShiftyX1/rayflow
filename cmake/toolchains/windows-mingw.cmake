# =============================================================================
# windows-mingw.cmake
# Toolchain for Windows with MinGW (MSYS2)
# =============================================================================

set(CMAKE_SYSTEM_NAME Windows)

# Compiler - using UCRT64 environment by default
# Adjust paths if using different MSYS2 environment (mingw64, clang64, etc.)
if(DEFINED ENV{MSYSTEM_PREFIX})
    set(MINGW_PREFIX "$ENV{MSYSTEM_PREFIX}")
else()
    set(MINGW_PREFIX "C:/msys64/ucrt64")
endif()

set(CMAKE_C_COMPILER "${MINGW_PREFIX}/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "${MINGW_PREFIX}/bin/g++.exe")
set(CMAKE_RC_COMPILER "${MINGW_PREFIX}/bin/windres.exe")

# Where to look for libraries and headers
set(CMAKE_FIND_ROOT_PATH "${MINGW_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Compiler flags
set(CMAKE_C_FLAGS_INIT "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_INIT "-Wall -Wextra -Wpedantic")

# Debug flags
set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0")

# Release flags
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")

# Static linking for easier distribution (no MinGW DLLs needed)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

# Windows-specific defines
add_compile_definitions(
    _WIN32_WINNT=0x0601
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
